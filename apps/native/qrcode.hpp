#pragma once
#include <QQuickImageProvider>
#include <QClipboard>
#include <QGuiApplication>
#include <QDesktopServices>
#include <QUrl>
#include <QTimer>
#ifdef Q_OS_ANDROID
#include <QJniObject>
#endif
class UiBridge : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString lastReceived READ lastReceived NOTIFY receivedChanged)
public:
    explicit UiBridge(QObject* parent = nullptr) : QObject(parent) {
#ifdef Q_OS_ANDROID
        scanner_.setInterval(250);
        connect(&scanner_, &QTimer::timeout, this, [this] {
            const auto value = QNativeInterface::QAndroidApplication::context()
                .callObjectMethod("takeScanResult", "()Ljava/lang/String;").toString();
            if (value.isEmpty()) return;
            scanner_.stop();
            if (value != "\n") emit invitationScanned(value);
        });
#endif
    }
    Q_INVOKABLE void copy(const QString& value) { QGuiApplication::clipboard()->setText(value); }
    Q_INVOKABLE QString paste() { return QGuiApplication::clipboard()->text(); }
    Q_INVOKABLE void scanInvitation() {
#ifdef Q_OS_ANDROID
        QNativeInterface::QAndroidApplication::context().callMethod<void>("scanInvitation", "()V");
        scanner_.start();
#endif
    }
    QString lastReceived() const { return received_; }
    void setReceived(const QString& path) { received_ = path; emit receivedChanged(); }
    Q_INVOKABLE void exportReceived() {
        if (received_.isEmpty()) return;
#ifdef Q_OS_ANDROID
        QNativeInterface::QAndroidApplication::context().callMethod<void>("exportFile", "(Ljava/lang/String;)V",
            QJniObject::fromString(received_).object<jstring>());
#else
        QDesktopServices::openUrl(QUrl::fromLocalFile(received_));
#endif
    }
signals:
    void receivedChanged();
    void invitationScanned(QString invitation);
private:
    QString received_;
    QTimer scanner_;
};
class QrProvider : public QQuickImageProvider {
public:
    QrProvider() : QQuickImageProvider(QQuickImageProvider::Image) {}
    QImage requestImage(const QString& id, QSize* size, const QSize& requested) override;
};
