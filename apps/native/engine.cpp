#include "engine.hpp"
#include "mdns.hpp"
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QHostInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLockFile>
#include <QNetworkInterface>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QStorageInfo>
#include <QUrl>
#include <QUrlQuery>
#include <QUuid>
#include <QtConcurrent>
#include <openssl/crypto.h>
#include <openssl/rand.h>
#include <stdexcept>
#ifdef Q_OS_ANDROID
#include <QJniObject>
#include <QCoreApplication>
#endif

namespace {
constexpr quint16 discoveryPort = 45439;
constexpr quint64 maxSize = 1024ULL * 1024 * 1024 * 1024; // 1 TiB per file
constexpr qsizetype headerLimit = 16384;
QString localAddress() {
    for (const auto& iface : QNetworkInterface::allInterfaces()) {
        if (!(iface.flags() & QNetworkInterface::IsUp) ||
            (iface.flags() & QNetworkInterface::IsLoopBack)) continue;
        for (const auto& entry : iface.addressEntries())
            if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol)
                return entry.ip().toString();
    }
    return "127.0.0.1";
}
QString sizeText(quint64 size) {
    return QLocale().formattedDataSize(static_cast<qint64>(size));
}
bool validHash(const QString& hash) {
    static const QRegularExpression regex("^[a-f0-9]{64}$");
    return regex.match(hash).hasMatch();
}
bool safeName(const QString& name) {
    static const QRegularExpression forbidden("[<>:\"/\\\\|?*\\x00-\\x1f]");
    const auto stem = name.section('.', 0, 0).toUpper();
    static const QRegularExpression reserved("^(CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])$");
    return !name.isEmpty() && name.size() <= 180 && name != "." && name != ".." &&
        !name.startsWith('.') && !name.endsWith('.') && !name.endsWith(' ') &&
        !forbidden.match(name).hasMatch() && !reserved.match(stem).hasMatch();
}
bool sameToken(const QString& offered, const QString& expected) {
    const auto a = offered.toLatin1(), b = expected.toLatin1();
    return a.size() == 64 && b.size() == 64 && CRYPTO_memcmp(a.constData(), b.constData(), 64) == 0;
}
}
Engine::Engine(const QString& directory, QObject* parent) : QObject(parent) {
    dataDirectory_ = directory.isEmpty()
        ? QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) : directory;
    if (!QDir().mkpath(dataDirectory_)) throw std::runtime_error("Cannot create app storage");
    settings_ = std::make_unique<QSettings>(dataDirectory_ + "/settings.ini", QSettings::IniFormat);
    identity_ = Identity::loadOrCreate(dataDirectory_ + "/identity");
    name_ = settings_->value("deviceName", QHostInfo::localHostName()).toString();
    folder_ = settings_->value("receiveFolder", QStandardPaths::writableLocation(
        QStandardPaths::DownloadLocation) + "/Quicky").toString();
#ifdef Q_OS_ANDROID
    folder_ = dataDirectory_ + "/received";
#endif
    dark_ = settings_->value("darkMode", true).toBool();
    onlyTrusted_ = settings_->value("onlyTrusted", false).toBool();
    trusted_ = QJsonDocument::fromJson(settings_->value("trustedPeers").toByteArray()).object();
    const auto doc = QJsonDocument::fromJson(settings_->value("history").toByteArray());
    history_ = doc.array().toVariantList();
    server_.acceptDescriptor = [this](qintptr descriptor) { acceptSocket(descriptor); };
    connect(&beacon_, &QTimer::timeout, this, &Engine::advertise);
    connect(&expiry_, &QTimer::timeout, this, &Engine::expireDevices);
    connect(&discovery_, &QUdpSocket::readyRead, this, &Engine::readDiscovery);
    connect(&mdns_, &QUdpSocket::readyRead, this, &Engine::readMdns);
    renewInvitation();
}
Engine::~Engine() {
    // Disconnect callbacks before member destruction; QObject children die later.
    for (const auto& socket : findChildren<QSslSocket*>()) {
        socket->disconnect(this);
        socket->abort();
    }
    if (incoming_) { incoming_->disconnect(this); incoming_->abort(); }
    if (outgoing_) { outgoing_->disconnect(this); outgoing_->abort(); }
}
bool Engine::start(quint16 port, bool discover) {
    if (!QSslSocket::supportsSsl()) { fail("TLS ist nicht verfügbar. OpenSSL-Installation prüfen."); return false; }
    if (!server_.listen(QHostAddress::AnyIPv4, port)) {
        fail("Empfang konnte nicht gestartet werden: " + server_.errorString()); return false;
    }
    if (discover) {
        if (mdns_.bind(QHostAddress::AnyIPv4, 5353,
                QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint)) {
            mdns_.setSocketOption(QAbstractSocket::MulticastTtlOption, 255);
            for (const auto& iface : QNetworkInterface::allInterfaces())
                if ((iface.flags() & QNetworkInterface::IsUp) && (iface.flags() & QNetworkInterface::CanMulticast))
                    mdns_.joinMulticastGroup(QHostAddress("224.0.0.251"), iface);
            mdns_.writeDatagram(mdns::query(), QHostAddress("224.0.0.251"), 5353);
        }
        if (discovery_.bind(QHostAddress::AnyIPv4, discoveryPort,
                            QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint)) {
            beacon_.start(3000); advertise();
        } else {
            detail_ = "Automatische Suche nicht verfügbar. QR-Einladung weiterhin möglich.";
        }
    }
    expiry_.start(1000);
    emit invitationChanged(); emit settingsChanged(); emit changed();
    return true;
}
bool Engine::busy() const {
    return incoming_ || outgoing_ || state_ == "hashing" || state_ == "paused" ||
        state_ == "verifying" || state_ == "reconnecting";
}
QString Engine::invitation() const {
    if (!visible_ || !server_.isListening() || QDateTime::currentSecsSinceEpoch() > expires_) return {};
    QUrl url; url.setScheme("quicky"); url.setHost("pair");
    QUrlQuery q;
    q.addQueryItem("host", localAddress());
    q.addQueryItem("port", QString::number(server_.serverPort()));
    q.addQueryItem("fp", identity_.fingerprint);
    q.addQueryItem("token", token_);
    url.setQuery(q);
    return url.toString(QUrl::FullyEncoded);
}
void Engine::setDeviceName(const QString& value) {
    const auto trimmed = value.trimmed().left(60);
    if (trimmed.isEmpty()) return;
    name_ = trimmed; settings_->setValue("deviceName", name_);
    emit settingsChanged(); advertise();
}
void Engine::setReceiveFolder(const QString& value) {
    if (busy()) return;
    const QUrl url(value);
    const auto path = url.isLocalFile() ? url.toLocalFile() : value;
    if (path.isEmpty() || !QDir().mkpath(path)) { fail("Empfangsordner nicht verfügbar."); return; }
    folder_ = QDir(path).absolutePath(); settings_->setValue("receiveFolder", folder_);
    emit settingsChanged();
}
void Engine::setVisible(bool value) {
    visible_ = value;
    if (!value && incoming_ && !receiverAccepted_) acceptIncoming(false);
    emit settingsChanged(); emit invitationChanged(); advertise();
}
void Engine::setDarkMode(bool value) {
    dark_ = value; settings_->setValue("darkMode", value); emit settingsChanged();
}
void Engine::setOnlyTrusted(bool value) {
    onlyTrusted_ = value; settings_->setValue("onlyTrusted", value); emit settingsChanged();
}
QVariantList Engine::pairedDevices() const {
    QVariantList list;
    for (auto it = trusted_.begin(); it != trusted_.end(); ++it) {
        auto item = it.value().toObject().toVariantMap();
        item["fingerprint"] = it.key(); list += item;
    }
    return list;
}
void Engine::rememberDevice(const QString& fp, const QString& name, const QString& host, quint16 port) {
    if (!validHash(fp) || !port) return;
    trusted_[fp] = QJsonObject{{"name", name.left(60)}, {"host", host}, {"port", port}};
    settings_->setValue("trustedPeers", QJsonDocument(trusted_).toJson(QJsonDocument::Compact));
    emit settingsChanged();
}
void Engine::forgetDevice(const QString& fp) {
    trusted_.remove(fp);
    settings_->setValue("trustedPeers", QJsonDocument(trusted_).toJson(QJsonDocument::Compact));
    emit settingsChanged();
}
void Engine::sendToDevice(const QString& fp, const QVariantList& files) {
    if (!trusted_.contains(fp)) return;
    auto device = trusted_[fp].toObject().toVariantMap();
    if (peers_.contains(fp)) device = peers_[fp].data;
    QUrl url; url.setScheme("quicky"); url.setHost("pair"); QUrlQuery q;
    q.addQueryItem("host", device["host"].toString());
    q.addQueryItem("port", device["port"].toString());
    q.addQueryItem("fp", fp); q.addQueryItem("token", QString(64, '0'));
    url.setQuery(q); sendFiles(files, url.toString());
}
void Engine::renewInvitation() {
    unsigned char random[32];
    if (RAND_bytes(random, sizeof random) != 1) throw std::runtime_error("Secure random unavailable");
    token_ = QByteArray(reinterpret_cast<char*>(random), sizeof random).toHex();
    expires_ = QDateTime::currentSecsSinceEpoch() + 300;
    emit invitationChanged();
}
QVariantList Engine::devices() const {
    QVariantList list;
    for (const auto& peer : peers_) list.append(peer.data);
    return list;
}
void Engine::advertise() {
    if (!visible_ || !server_.isListening()) return;
    if (mdns_.isValid()) {
        const auto packet = mdns::announce({name_, QSysInfo::productType(), identity_.fingerprint, port()}, localAddress());
        mdns_.writeDatagram(packet, QHostAddress("224.0.0.251"), 5353);
    }
    if (!discovery_.isValid()) return;
    const auto bytes = QJsonDocument(QJsonObject{
        {"app", "quicky"}, {"v", 1}, {"name", name_},
        {"fp", identity_.fingerprint}, {"port", server_.serverPort()},
        {"platform", QSysInfo::productType()}}).toJson(QJsonDocument::Compact);
    discovery_.writeDatagram(bytes, QHostAddress::Broadcast, discoveryPort);
}
void Engine::readDiscovery() {
    while (discovery_.hasPendingDatagrams()) {
        if (discovery_.pendingDatagramSize() > 2048) {
            discovery_.readDatagram(nullptr, 0); continue;
        }
        QByteArray data(static_cast<int>(discovery_.pendingDatagramSize()), Qt::Uninitialized);
        QHostAddress sender;
        discovery_.readDatagram(data.data(), data.size(), &sender);
        const auto object = QJsonDocument::fromJson(data).object();
        const auto fp = object["fp"].toString();
        if (object["app"] != "quicky" || object["v"].toInt() != 1 ||
            fp == identity_.fingerprint || !validHash(fp) || peers_.size() > 128) continue;
        peers_[fp] = {{ {"name", object["name"].toString().left(60)},
                        {"platform", object["platform"].toString().left(30)},
                        {"host", sender.toString()}, {"port", object["port"].toInt()}, {"fingerprint", fp}},
                      QDateTime::currentSecsSinceEpoch()};
        emit devicesChanged();
    }
}
void Engine::readMdns() {
    while (mdns_.hasPendingDatagrams()) {
        if (mdns_.pendingDatagramSize() > 9000) { mdns_.readDatagram(nullptr, 0); continue; }
        QByteArray bytes(static_cast<int>(mdns_.pendingDatagramSize()), Qt::Uninitialized);
        QHostAddress sender;
        mdns_.readDatagram(bytes.data(), bytes.size(), &sender);
        if (mdns::isQuery(bytes) && QDateTime::currentMSecsSinceEpoch() - mdnsLastResponse_ > 1000) {
            mdnsLastResponse_ = QDateTime::currentMSecsSinceEpoch(); advertise();
        }
        for (const auto& device : mdns::parse(bytes)) {
            if (device.fingerprint == identity_.fingerprint || peers_.size() >= 128) continue;
            peers_[device.fingerprint] = {{{"name", device.name}, {"platform", device.platform},
                {"host", sender.toString()}, {"port", device.port}, {"fingerprint", device.fingerprint}}, QDateTime::currentSecsSinceEpoch()};
            emit devicesChanged();
        }
    }
}
void Engine::expireDevices() {
    const auto now = QDateTime::currentSecsSinceEpoch();
    bool removed = false;
    for (auto it = peers_.begin(); it != peers_.end();) {
        if (now - it->seen > 12) { it = peers_.erase(it); removed = true; }
        else ++it;
    }
    if (removed) emit devicesChanged();
    if (now > expires_) { token_.clear(); emit invitationChanged(); }
}
void Engine::sendJson(QSslSocket* socket, const QJsonObject& message) {
    socket->write(QJsonDocument(message).toJson(QJsonDocument::Compact) + '\n');
}
void Engine::update(const QString& state, const QString& status, const QString& detail) {
    state_ = state; status_ = status; detail_ = detail; emit changed();
}
void Engine::fail(const QString& message) {
    ++operation_;
    queue_.clear();
    resetSender();
    if (incoming_ && incoming_->isEncrypted())
        sendJson(incoming_, {{"type", "error"}, {"message", message.left(200)}});
    cleanupReceive(false);
    update("failed", "Verbindung unterbrochen", message);
    emit failed(message);
}
void Engine::addHistory(const QString& name, const QString& direction, quint64 bytes) {
    history_.prepend(QVariantMap{{"name", name}, {"direction", direction},
        {"size", sizeText(bytes)}, {"time", QDateTime::currentDateTime().toString(Qt::ISODate)}});
    while (history_.size() > 50) history_.removeLast();
    settings_->setValue("history", QJsonDocument(QJsonArray::fromVariantList(history_)).toJson(QJsonDocument::Compact));
    emit historyChanged();
}
void Engine::clearHistory() { history_.clear(); settings_->remove("history"); emit historyChanged(); }
void Engine::openReceiveFolder() { emit openFolderRequested(folder_); }

void Engine::acceptSocket(qintptr descriptor) {
    auto* socket = new QSslSocket(this);
    if (!socket->setSocketDescriptor(descriptor)) { socket->deleteLater(); return; }
    // Bound resource use before doing expensive handshakes.
    if (!visible_ || busy() || handshakes_.size() >= 8) {
        socket->abort(); socket->deleteLater(); return;
    }
    handshakes_.append(socket);
    socket->setReadBufferSize(headerLimit + 1);
    socket->setProtocol(QSsl::TlsV1_3OrLater);
    // Request the sender certificate. Out-of-band token plus consent establishes
    // initial trust; later sessions require the exact saved certificate digest.
    socket->setPeerVerifyMode(QSslSocket::QueryPeer);
    socket->setLocalCertificate(identity_.certificate);
    socket->setPrivateKey(identity_.privateKey);
    auto* timeout = new QTimer(socket);
    timeout->setSingleShot(true); timeout->start(15000);
    connect(timeout, &QTimer::timeout, socket, &QSslSocket::abort);
    connect(socket, &QSslSocket::readyRead, this, [this, socket] { receiveOffer(socket); });
    connect(socket, &QSslSocket::disconnected, this, [this, socket] {
        handshakes_.removeAll(socket);
        if (incoming_ == socket && state_ != "verifying") {
            cleanupReceive(false);
            update("idle", "Empfang unterbrochen", "Der Sender kann die Datei erneut anbieten und fortsetzen.");
        }
        socket->deleteLater();
    });
    socket->startServerEncryption();
}
void Engine::receiveOffer(QSslSocket* socket) {
    if (incoming_ == socket && receiverAccepted_) { receiveBody(); return; }
    if (incoming_ == socket) return;
    if (!socket->isEncrypted()) return;
    if (socket->bytesAvailable() > headerLimit || !socket->canReadLine()) {
        if (socket->bytesAvailable() > headerLimit) socket->abort();
        return;
    }
    const auto line = socket->readLine(headerLimit);
    const auto object = QJsonDocument::fromJson(line).object();
    bool sizeOk = false;
    const auto size = object["size"].toString().toULongLong(&sizeOk);
    const auto name = object["name"].toString();
    const auto clientCertificate = socket->peerCertificate();
    const auto clientFp = QString::fromLatin1(clientCertificate.digest(QCryptographicHash::Sha256).toHex());
    const bool trusted = !clientCertificate.isNull() && trusted_.contains(clientFp) &&
        clientCertificate.expiryDate() > QDateTime::currentDateTimeUtc();
    const bool invited = !onlyTrusted_ && QDateTime::currentSecsSinceEpoch() <= expires_ &&
        sameToken(object["token"].toString(), token_);
    if (object["v"].toInt() != 1 || object["type"] != "offer" ||
        !visible_ || (!trusted && !invited) || clientCertificate.isNull() || !sizeOk || size > maxSize ||
        !safeName(name) || !validHash(object["sha256"].toString()) || busy()) {
        sendJson(socket, {{"type", "error"}, {"message", "Angebot abgelehnt oder Einladung abgelaufen."}});
        socket->disconnectFromHost(); return;
    }
    incoming_ = socket;
    incomingFingerprint_ = clientFp;
    rememberIncoming_ = trusted;
    handshakes_.removeAll(socket);
    for (auto* timer : socket->findChildren<QTimer*>()) timer->start(60000);
    pendingOffer_ = object;
    receiveName_ = name; receiveHash_ = object["sha256"].toString();
    senderName_ = object["sender"].toString().left(60);
    total_ = size; done_ = 0;
    update("offered", "Datei empfangen?", name + " · " + sizeText(size));
    emit incomingOffer(name, senderName_, sizeText(size));
}
void Engine::acceptIncoming(bool accepted, bool remember) {
    if (!incoming_ || receiverAccepted_) return;
    if (!accepted) {
        sendJson(incoming_, {{"type", "error"}, {"message", "Empfang abgelehnt."}});
        cleanupReceive(false); update("idle", "Empfang abgelehnt"); return;
    }
    rememberIncoming_ = rememberIncoming_ || remember;
    const auto partDir = folder_ + "/.quicky-partials";
    if (!QDir().mkpath(partDir)) { fail("Empfangsordner kann nicht angelegt werden."); return; }
    partialPath_ = partDir + "/" + receiveHash_ + ".part";
    partialLock_ = std::make_unique<QLockFile>(partialPath_ + ".lock");
    if (!partialLock_->tryLock(0)) { fail("Diese Datei wird bereits empfangen."); return; }
    const QFileInfo partInfo(partialPath_);
    if (partInfo.isSymLink()) { fail("Unsichere temporäre Datei."); return; }
    receiveFile_.setFileName(partialPath_);
    if (!receiveFile_.open(QIODevice::ReadWrite)) { fail("Temporäre Datei kann nicht geöffnet werden."); return; }
    receiveFile_.setPermissions(QFile::ReadOwner | QFile::WriteOwner);
    done_ = static_cast<quint64>(receiveFile_.size());
    if (done_ > total_) { if (!receiveFile_.resize(0)) { fail("Temporäre Datei kann nicht zurückgesetzt werden."); return; } done_ = 0; }
    const auto available = QStorageInfo(folder_).bytesAvailable();
    if (available >= 0 && static_cast<quint64>(available) < total_ - done_ + 1024 * 1024) {
        fail("Nicht genügend freier Speicherplatz."); return;
    }
    if (!receiveFile_.seek(static_cast<qint64>(done_))) { fail("Dateiposition ungültig."); return; }
    receiverAccepted_ = true;
    incoming_->setReadBufferSize(512 * 1024);
    for (auto* timer : incoming_->findChildren<QTimer*>()) timer->start(60000);
    update("receiving", "Empfange …", receiveName_);
    if (!incoming_ || !receiverAccepted_) return;
    sendJson(incoming_, {{"type", "ready"}, {"offset", QString::number(done_)}});
    receiveBody();
}
void Engine::receiveBody() {
    if (!incoming_ || !receiverAccepted_ || state_ == "verifying") return;
    while (incoming_ && incoming_->bytesAvailable() && done_ < total_) {
        const auto bytes = incoming_->read(static_cast<qint64>(qMin<quint64>(256 * 1024, total_ - done_)));
        if (receiveFile_.write(bytes) != bytes.size()) { fail("Schreiben fehlgeschlagen. Speicherplatz prüfen."); return; }
        done_ += static_cast<quint64>(bytes.size());
        for (auto* timer : incoming_->findChildren<QTimer*>()) timer->start(60000);
    }
    emit changed();
    if (done_ == total_) finalizeIncoming();
}
QByteArray Engine::hashFile(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return {};
    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (!hash.addData(&file)) return {};
    return hash.result().toHex();
}
void Engine::finalizeIncoming() {
    if (!receiveFile_.flush()) { fail("Datei konnte nicht gespeichert werden."); return; }
    receiveFile_.close();
    update("verifying", "Datei wird geprüft …", receiveName_);
    if (!incoming_) return;
    for (auto* timer : incoming_->findChildren<QTimer*>()) timer->stop();
    const auto operation = ++operation_;
    auto* watcher = new QFutureWatcher<QByteArray>(this);
    auto* keepalive = new QTimer(watcher);
    keepalive->start(10000);
    connect(keepalive, &QTimer::timeout, this, [this, operation] {
        if (operation == operation_ && incoming_)
            sendJson(incoming_, {{"type", "checking"}});
    });
    connect(watcher, &QFutureWatcher<QByteArray>::finished, this, [this, watcher, operation] {
        const auto digest = watcher->result(); watcher->deleteLater();
        if (operation != operation_) return;
        if (digest != receiveHash_.toLatin1()) {
            if (incoming_) sendJson(incoming_, {{"type", "error"}, {"message", "Integritätsprüfung fehlgeschlagen."}});
            cleanupReceive(true); fail("Integritätsprüfung fehlgeschlagen. Datei wurde verworfen."); return;
        }
        auto path = folder_ + "/" + receiveName_;
        if (QFile::exists(path)) {
            const QFileInfo info(receiveName_);
            path = folder_ + "/" + info.completeBaseName() + "-" +
                QUuid::createUuid().toString(QUuid::Id128).left(8) +
                (info.suffix().isEmpty() ? QString() : "." + info.suffix());
        }
        if (!QFile::rename(partialPath_, path)) { fail("Datei kann nicht im Zielordner abgelegt werden."); return; }
        if (incoming_ && rememberIncoming_) {
            const auto rawPort = pendingOffer_["listenPort"].toInt();
            if (rawPort > 0 && rawPort <= 65535)
                rememberDevice(incomingFingerprint_, senderName_, incoming_->peerAddress().toString(), quint16(rawPort));
        }
        if (incoming_) sendJson(incoming_, {{"type", "done"}, {"sha256", receiveHash_},
            {"remembered", rememberIncoming_}, {"receiver", name_}});
        addHistory(receiveName_, "Empfangen", total_);
        cleanupReceive(false);
        update("completed", "Übertragung abgeschlossen", receiveName_);
        emit finished(path);
    });
    watcher->setFuture(QtConcurrent::run(&Engine::hashFile, partialPath_));
}
void Engine::cleanupReceive(bool removePartial) {
    receiveFile_.close();
    if (incoming_) {
        auto socket = incoming_; incoming_ = nullptr;
        socket->disconnectFromHost();
    }
    if (removePartial && !partialPath_.isEmpty()) QFile::remove(partialPath_);
    partialLock_.reset(); receiverAccepted_ = false; pendingOffer_ = {};
}
void Engine::sendFiles(const QVariantList& urls, const QString& invitation) {
    if (busy()) return;
    queue_.clear();
    retries_ = 0;
    for (const auto& value : urls) {
        const QUrl url(value.toString());
        const auto path = url.isLocalFile() ? url.toLocalFile() : value.toString();
        // QFile supports Android content:// URIs through Qt's content file engine.
        if (!path.isEmpty()) queue_.append(path);
    }
    if (queue_.isEmpty()) { fail("Keine Dateien ausgewählt."); return; }
    targetInvitation_ = invitation.trimmed();
    beginNext();
}
void Engine::sendText(const QString& text, const QString& invitation) {
    if (busy() || text.isEmpty()) return;
    if (text.toUtf8().size() > 1024 * 1024) { fail("Text darf höchstens 1 MiB groß sein."); return; }
    const auto path = dataDirectory_ + "/Text-" + QUuid::createUuid().toString(QUuid::Id128).left(8) + ".txt";
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(text.toUtf8()) != text.toUtf8().size()) {
        fail("Text konnte nicht vorbereitet werden."); return;
    }
    file.close(); sendFiles({QUrl::fromLocalFile(path)}, invitation);
}
void Engine::beginNext() {
    resetSender();
    if (queue_.isEmpty()) return;
    sourcePath_ = queue_.takeFirst();
    QFile probe(sourcePath_);
    if (!probe.open(QIODevice::ReadOnly)) { fail("Quelldatei kann nicht geöffnet werden."); return; }
    total_ = static_cast<quint64>(probe.size()); done_ = 0;
    sourceName_ = QFileInfo(sourcePath_).fileName();
#ifdef Q_OS_ANDROID
    if (sourcePath_.startsWith("content:")) {
        const auto activity = QNativeInterface::QAndroidApplication::context();
        sourceName_ = activity.callObjectMethod("displayName", "(Ljava/lang/String;)Ljava/lang/String;",
            QJniObject::fromString(sourcePath_).object<jstring>()).toString();
    }
#endif
    if (!safeName(sourceName_) || total_ > maxSize) { fail("Dateiname oder Dateigröße nicht unterstützt."); return; }
    probe.close();
    update("hashing", "Datei wird vorbereitet …", sourceName_);
    const auto operation = ++operation_;
    auto* watcher = new QFutureWatcher<QByteArray>(this);
    connect(watcher, &QFutureWatcher<QByteArray>::finished, this, [this, watcher, operation] {
        const auto hash = watcher->result(); watcher->deleteLater();
        if (operation != operation_) return;
        if (hash.size() != 64) { fail("Datei kann nicht vollständig gelesen werden."); return; }
        sourceHash_ = QString::fromLatin1(hash);
        connectSender();
    });
    watcher->setFuture(QtConcurrent::run(&Engine::hashFile, sourcePath_));
}
void Engine::connectSender() {
    const QUrl url(targetInvitation_);
    const QUrlQuery query(url);
    bool portOk = false;
    const auto rawPort = query.queryItemValue("port").toUInt(&portOk);
    const auto host = query.queryItemValue("host");
    const auto fp = query.queryItemValue("fp");
    const auto token = query.queryItemValue("token");
    QHostAddress address(host);
    if (url.scheme() != "quicky" || url.host() != "pair" || !portOk || rawPort == 0 ||
        rawPort > 65535 || address.isNull() || !validHash(fp) || !validHash(token)) {
        fail("Ungültige Quicky-Einladung. Vollständigen QR-Link einfügen."); return;
    }
    sendFile_.setFileName(sourcePath_);
    if (!sendFile_.open(QIODevice::ReadOnly) || static_cast<quint64>(sendFile_.size()) != total_) {
        fail("Quelldatei wurde verändert oder ist nicht mehr verfügbar."); return;
    }
    auto* socket = new QSslSocket(this);
    outgoing_ = socket;
    socket->setProtocol(QSsl::TlsV1_3OrLater);
    socket->setPeerVerifyMode(QSslSocket::VerifyPeer);
    socket->setLocalCertificate(identity_.certificate);
    socket->setPrivateKey(identity_.privateKey);
    socket->setReadBufferSize(headerLimit + 1);
    connect(socket, &QSslSocket::sslErrors, this, [this, socket, fp](const QList<QSslError>& errors) {
        if (QString::fromLatin1(socket->peerCertificate().digest(QCryptographicHash::Sha256).toHex()) != fp) {
            fail("Geräteidentität stimmt nicht mit der Einladung überein."); return;
        }
        // A QR-pinned self-signed identity replaces CA/hostname validation only.
        // Expiry, invalid signatures and all other TLS errors remain fatal.
        for (const auto& error : errors) {
            if (error.error() != QSslError::SelfSignedCertificate &&
                error.error() != QSslError::HostNameMismatch &&
                error.error() != QSslError::CertificateUntrusted) return;
        }
        socket->ignoreSslErrors(errors);
    });
    auto* timeout = new QTimer(socket);
    timeout->setSingleShot(true); timeout->start(60000);
    connect(timeout, &QTimer::timeout, this, [this, socket] {
        if (outgoing_ == socket) retrySender("Zeitüberschreitung.");
    });
    connect(socket, &QSslSocket::encrypted, this, [this, socket, fp, token] {
        if (outgoing_ != socket) return;
        if (QString::fromLatin1(socket->peerCertificate().digest(QCryptographicHash::Sha256).toHex()) != fp) {
            fail("Geräteidentität stimmt nicht mit der Einladung überein."); return;
        }
        sendJson(socket, {{"v", 1}, {"type", "offer"}, {"token", token},
            {"name", sourceName_}, {"size", QString::number(total_)},
            {"sha256", sourceHash_}, {"sender", name_}, {"listenPort", port()}});
        update("connecting", "Warte auf Bestätigung …", sourceName_);
    });
    connect(socket, &QSslSocket::readyRead, this, &Engine::senderReply);
    connect(socket, &QSslSocket::bytesWritten, this, [this, socket, timeout](qint64) {
        if (outgoing_ != socket) return;
        timeout->start(60000); pumpSend();
    });
    connect(socket, &QSslSocket::errorOccurred, this, [this, socket](QAbstractSocket::SocketError error) {
        if (outgoing_ == socket && error != QAbstractSocket::RemoteHostClosedError) {
            const auto message = "TLS-Verbindung fehlgeschlagen: " + socket->errorString();
            if (error == QAbstractSocket::SslHandshakeFailedError) fail(message);
            else retrySender(message);
        }
    });
    connect(socket, &QSslSocket::disconnected, this, [this, socket] {
        if (outgoing_ == socket) retrySender("Empfänger hat die Verbindung geschlossen.");
        socket->deleteLater();
    });
    update("connecting", "Sichere Verbindung wird aufgebaut …", sourceName_);
    socket->connectToHostEncrypted(host, static_cast<quint16>(rawPort));
}
void Engine::senderReply() {
    if (!outgoing_) return;
    replyBuffer_ += outgoing_->readAll();
    for (auto* timer : outgoing_->findChildren<QTimer*>()) timer->start(60000);
    if (replyBuffer_.size() > headerLimit) { fail("Ungültige Protokollantwort."); return; }
    while (outgoing_ && replyBuffer_.contains('\n')) {
        const auto end = replyBuffer_.indexOf('\n');
        const auto object = QJsonDocument::fromJson(replyBuffer_.left(end)).object();
        replyBuffer_.remove(0, end + 1);
        if (object["type"] == "error") { fail(object["message"].toString().left(200)); return; }
        if (object["type"] == "checking" && waitingDone_) {
            continue;
        } else if (object["type"] == "ready" && !senderReady_) {
            bool ok = false; const auto offset = object["offset"].toString().toULongLong(&ok);
            if (!ok || offset > total_ || !sendFile_.seek(static_cast<qint64>(offset))) {
                fail("Ungültige Wiederaufnahmeposition."); return;
            }
            done_ = offset; senderReady_ = true;
            update("sending", "Sende …", sourceName_); pumpSend();
        } else if (object["type"] == "done" && waitingDone_ && object["sha256"] == sourceHash_) {
            const auto name = sourceName_;
            if (object["remembered"].toBool()) {
                QUrlQuery invitationQuery{QUrl(targetInvitation_)};
                rememberDevice(invitationQuery.queryItemValue("fp"), object["receiver"].toString(),
                    invitationQuery.queryItemValue("host"), quint16(invitationQuery.queryItemValue("port").toUInt()));
            }
            addHistory(name, "Gesendet", total_);
            retries_ = 0;
            resetSender();
            if (!queue_.isEmpty()) { beginNext(); return; }
            update("completed", "Übertragung abgeschlossen", name);
            emit finished(name); return;
        } else { fail("Unerwartete Protokollantwort."); return; }
    }
}
void Engine::pumpSend() {
    if (!outgoing_ || !senderReady_ || waitingDone_ || state_ != "sending") return;
    while (done_ < total_ && outgoing_->bytesToWrite() < 512 * 1024) {
        const auto count = static_cast<qint64>(qMin<quint64>(256 * 1024, total_ - done_));
        const auto bytes = sendFile_.read(count);
        if (bytes.size() != count || outgoing_->write(bytes) != count) {
            fail("Datei konnte nicht gesendet werden."); return;
        }
        done_ += static_cast<quint64>(count);
    }
    emit changed();
    if (done_ == total_) {
        waitingDone_ = true;
        update("sending", "Empfänger prüft die Datei …", sourceName_);
    }
}
void Engine::resetSender() {
    if (outgoing_) {
        auto socket = outgoing_; outgoing_ = nullptr;
        socket->disconnect(this); socket->abort(); socket->deleteLater();
    }
    sendFile_.close(); replyBuffer_.clear(); senderReady_ = false; waitingDone_ = false;
}
void Engine::retrySender(const QString& reason) {
    if (++retries_ > 3) { fail(reason + " Drei Wiederholungen fehlgeschlagen."); return; }
    resetSender();
    const auto operation = ++operation_;
    update("reconnecting", "Verbindung wird wiederhergestellt …",
        reason + " Versuch " + QString::number(retries_) + " von 3.");
    QTimer::singleShot(1000 * (1 << (retries_ - 1)), this, [this, operation] {
        if (operation != operation_ || state_ != "reconnecting") return;
        queue_.prepend(sourcePath_); beginNext();
    });
}
void Engine::pause() {
    if (state_ != "sending" && state_ != "connecting") return;
    resetSender(); update("paused", "Übertragung pausiert", sourceName_);
}
void Engine::resume() {
    if (state_ != "paused") return;
    // Rehash on resume so a changed source can never append to a different file.
    retries_ = 0; queue_.prepend(sourcePath_); beginNext();
}
void Engine::cancel() {
    ++operation_; queue_.clear(); resetSender(); cleanupReceive(true);
    update("cancelled", "Übertragung abgebrochen");
}
