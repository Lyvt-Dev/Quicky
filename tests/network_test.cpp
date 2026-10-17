#include "engine.hpp"
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QTemporaryDir>
#include <QThread>
#include <QUrlQuery>
#include <iostream>
#include <stdexcept>

void require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
void waitUntil(const std::function<bool()>& done, int timeout = 15000) {
    QElapsedTimer timer; timer.start();
    while (!done() && timer.elapsed() < timeout) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        QThread::msleep(1);
    }
    require(done(), "Timed out waiting for network state");
}
QString invite(Engine& receiver, const QString& field = {}, const QString& replacement = {}) {
    QUrl url(receiver.invitation());
    QUrlQuery query(url); query.removeAllQueryItems("host"); query.addQueryItem("host", "127.0.0.1");
    if (!field.isEmpty()) { query.removeAllQueryItems(field); query.addQueryItem(field, replacement); }
    url.setQuery(query); return url.toString();
}
QByteArray read(const QString& path) { QFile file(path); require(file.open(QIODevice::ReadOnly), "Missing output file"); return file.readAll(); }
void write(const QString& path, const QByteArray& bytes) {
    QFile file(path); require(file.open(QIODevice::WriteOnly), "Cannot create fixture");
    require(file.write(bytes) == bytes.size(), "Cannot write fixture");
}
int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    app.setApplicationName("QuickyNetworkTests");
    try {
        QTemporaryDir temp; require(temp.isValid(), "Temporary directory unavailable");
        Engine receiver(temp.path() + "/receiver");
        Engine sender(temp.path() + "/sender");
        const auto folder = temp.path() + "/received";
        receiver.setReceiveFolder(folder);
        require(receiver.start(0, false) && sender.start(0, false), "TLS listeners failed");
        auto consent = QObject::connect(&receiver, &Engine::incomingOffer, &receiver,
            [&receiver] { receiver.acceptIncoming(true); });
        QByteArray payload(1300000, '\0');
        for (qsizetype i = 0; i < payload.size(); ++i) payload[i] = static_cast<char>(i % 251);
        const auto file = temp.path() + "/binary.dat";
        write(file, payload);
        sender.sendFiles({QUrl::fromLocalFile(file)}, invite(receiver));
        waitUntil([&] { return sender.state() == "completed" || sender.state() == "failed"; });
        if (sender.state() == "failed") std::cerr << sender.detail().toStdString() << '\n';
        require(sender.state() == "completed", "Encrypted binary transfer failed");
        require(read(folder + "/binary.dat") == payload, "Transferred bytes differ");
        require(sender.history().size() == 1 && receiver.history().size() == 1, "History missing");
        std::cout << "PASS TLS 1.3 binary transfer and history\n";

        const auto empty = temp.path() + "/empty.txt"; write(empty, {});
        sender.sendFiles({QUrl::fromLocalFile(empty)}, invite(receiver));
        waitUntil([&] { return sender.state() == "completed" || sender.state() == "failed"; });
        require(sender.state() == "completed" && read(folder + "/empty.txt").isEmpty(), "Empty file failed");
        std::cout << "PASS empty file\n";

        sender.sendFiles({QUrl::fromLocalFile(file)}, invite(receiver, "fp", QString(64, '0')));
        waitUntil([&] { return sender.state() == "failed"; });
        require(receiver.history().size() == 2, "Wrong fingerprint accepted");
        std::cout << "PASS wrong identity rejected\n";

        sender.sendFiles({QUrl::fromLocalFile(file)}, invite(receiver, "token", QString(64, '0')));
        waitUntil([&] { return sender.state() == "failed"; });
        require(receiver.history().size() == 2, "Wrong invitation accepted");
        std::cout << "PASS wrong token rejected\n";

        QObject::disconnect(consent);
        consent = QObject::connect(&receiver, &Engine::incomingOffer, &receiver,
            [&receiver] { receiver.acceptIncoming(false); });
        sender.sendFiles({QUrl::fromLocalFile(file)}, invite(receiver));
        waitUntil([&] { return sender.state() == "failed"; });
        require(receiver.history().size() == 2, "Declined transfer saved");
        std::cout << "PASS receiver refusal\n";
        QObject::disconnect(consent);
        consent = QObject::connect(&receiver, &Engine::incomingOffer, &receiver,
            [&receiver] { receiver.acceptIncoming(true); });

        const auto partialDir = folder + "/.quicky-partials";
        QDir().mkpath(partialDir);
        const auto hash = QString::fromLatin1(QCryptographicHash::hash(payload, QCryptographicHash::Sha256).toHex());
        write(partialDir + "/" + hash + ".part", payload.left(500000));
        sender.sendFiles({QUrl::fromLocalFile(file)}, invite(receiver));
        waitUntil([&] { return sender.state() == "completed" || sender.state() == "failed"; });
        require(sender.state() == "completed", "Resume from existing partial failed");
        require(QDir(folder).entryList({"binary*.dat"}, QDir::Files).size() == 2, "Existing target overwritten");
        std::cout << "PASS partial resume and collision-safe filename\n";

        write(partialDir + "/" + hash + ".part", QByteArray(500000, 'x'));
        sender.sendFiles({QUrl::fromLocalFile(file)}, invite(receiver));
        waitUntil([&] { return sender.state() == "failed"; });
        require(receiver.history().size() == 3, "Corrupt partial marked successful");
        require(!QFile::exists(partialDir + "/" + hash + ".part"), "Corrupt file retained");
        std::cout << "PASS content integrity failure\n";

        receiver.setVisible(false);
        sender.sendFiles({QUrl::fromLocalFile(file)}, invite(sender)); // unrelated token and certificate
        waitUntil([&] { return sender.state() == "failed"; });
        receiver.setVisible(true);
        sender.sendFiles({QUrl::fromLocalFile(file), QUrl::fromLocalFile(empty)}, invite(receiver));
        waitUntil([&] { return sender.state() == "completed" || sender.state() == "failed"; });
        require(sender.state() == "completed" && receiver.history().size() == 5, "Multi-file queue failed");
        std::cout << "PASS sequential multi-file queue\n";
        receiver.clearHistory(); require(receiver.history().isEmpty(), "History clearing failed");
        std::cout << "All encrypted integration scenarios passed\n";
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
