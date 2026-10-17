#pragma once
#include "identity.hpp"
#include <QObject>
#include <QFile>
#include <QJsonObject>
#include <QPointer>
#include <QSettings>
#include <QSslSocket>
#include <QTcpServer>
#include <QTimer>
#include <QUdpSocket>
#include <QVariantList>
#include <functional>

class TlsListener : public QTcpServer {
public:
    using QTcpServer::QTcpServer;
    std::function<void(qintptr)> acceptDescriptor;
protected:
    void incomingConnection(qintptr descriptor) override { acceptDescriptor(descriptor); }
};

class Engine : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString deviceName READ deviceName WRITE setDeviceName NOTIFY settingsChanged)
    Q_PROPERTY(QString receiveFolder READ receiveFolder WRITE setReceiveFolder NOTIFY settingsChanged)
    Q_PROPERTY(QString invitation READ invitation NOTIFY invitationChanged)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(QString detail READ detail NOTIFY changed)
    Q_PROPERTY(QString state READ state NOTIFY changed)
    Q_PROPERTY(double progress READ progress NOTIFY changed)
    Q_PROPERTY(QVariantList devices READ devices NOTIFY devicesChanged)
    Q_PROPERTY(QVariantList history READ history NOTIFY historyChanged)
    Q_PROPERTY(bool visible READ visible WRITE setVisible NOTIFY settingsChanged)
    Q_PROPERTY(bool darkMode READ darkMode WRITE setDarkMode NOTIFY settingsChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(bool listening READ listening NOTIFY settingsChanged)
    Q_PROPERTY(QString fingerprint READ fingerprint CONSTANT)
public:
    explicit Engine(const QString& dataDirectory = {}, QObject* parent = nullptr);
    ~Engine() override;
    bool start(quint16 port = 0, bool discover = true);
    QString deviceName() const { return name_; }
    QString receiveFolder() const { return folder_; }
    QString invitation() const;
    QString status() const { return status_; }
    QString detail() const { return detail_; }
    QString state() const { return state_; }
    double progress() const { return state_ == "completed" ? 1.0 : (total_ ? double(done_) / double(total_) : 0); }
    QVariantList devices() const;
    QVariantList history() const { return history_; }
    bool visible() const { return visible_; }
    bool darkMode() const { return dark_; }
    bool busy() const;
    bool listening() const { return server_.isListening(); }
    QString fingerprint() const { return identity_.fingerprint; }
    quint16 port() const { return server_.serverPort(); }
    void setDeviceName(const QString& value);
    void setReceiveFolder(const QString& value);
    void setVisible(bool value);
    void setDarkMode(bool value);
    Q_INVOKABLE void renewInvitation();
    Q_INVOKABLE void sendFiles(const QVariantList& urls, const QString& invitation);
    Q_INVOKABLE void sendText(const QString& text, const QString& invitation);
    Q_INVOKABLE void acceptIncoming(bool accepted);
    Q_INVOKABLE void pause();
    Q_INVOKABLE void resume();
    Q_INVOKABLE void cancel();
    Q_INVOKABLE void clearHistory();
    Q_INVOKABLE void openReceiveFolder();
signals:
    void changed();
    void settingsChanged();
    void invitationChanged();
    void devicesChanged();
    void historyChanged();
    void incomingOffer(QString name, QString sender, QString size);
    void finished(QString path);
    void failed(QString message);
    void openFolderRequested(QString path);
private:
    void acceptSocket(qintptr descriptor);
    void receiveOffer(QSslSocket* socket);
    void receiveBody();
    void finalizeIncoming();
    void beginNext();
    void connectSender();
    void senderReply();
    void pumpSend();
    void fail(const QString& message);
    void update(const QString& state, const QString& status, const QString& detail = {});
    void addHistory(const QString& name, const QString& direction, quint64 bytes);
    void advertise();
    void readDiscovery();
    void expireDevices();
    void cleanupReceive(bool removePartial);
    void resetSender();
    static void sendJson(QSslSocket* socket, const QJsonObject& message);
    static QByteArray hashFile(const QString& path);
    QString dataDirectory_;
    std::unique_ptr<QSettings> settings_;
    Identity identity_;
    QString name_, folder_, token_;
    qint64 expires_ = 0;
    bool visible_ = true, dark_ = true;
    QString status_ = "Bereit zum Teilen", detail_, state_ = "idle";
    quint64 done_ = 0, total_ = 0;
    quint64 operation_ = 0;
    TlsListener server_;
    QUdpSocket discovery_;
    QTimer beacon_, expiry_;
    struct Peer { QVariantMap data; qint64 seen; };
    QMap<QString, Peer> peers_;
    QVariantList history_;
    QPointer<QSslSocket> incoming_, outgoing_;
    QList<QPointer<QSslSocket>> handshakes_;
    QFile receiveFile_, sendFile_;
    QString receiveName_, receiveHash_, partialPath_, senderName_;
    QString sourcePath_, sourceName_, sourceHash_, targetInvitation_;
    QJsonObject pendingOffer_;
    QStringList queue_;
    QByteArray replyBuffer_;
    bool receiverAccepted_ = false, senderReady_ = false, waitingDone_ = false;
    std::unique_ptr<class QLockFile> partialLock_;
};
