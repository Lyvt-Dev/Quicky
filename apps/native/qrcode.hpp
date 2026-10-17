#pragma once
#include <QQuickImageProvider>
#include <QClipboard>
#include <QGuiApplication>
#include <QDesktopServices>
#include <QUrl>
#ifdef Q_OS_ANDROID
#include <QJniObject>
#endif
class UiBridge : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString lastReceived READ lastReceived NOTIFY receivedChanged)
public:
    using QObject::QObject;
    Q_INVOKABLE void copy(const QString& value) { QGuiApplication::clipboard()->setText(value); }
    Q_INVOKABLE QString paste() { return QGuiApplication::clipboard()->text(); }
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
private:
    QString received_;
};
class QrProvider : public QQuickImageProvider {
public:
    QrProvider() : QQuickImageProvider(QQuickImageProvider::Image) {}
    QImage requestImage(const QString& id, QSize* size, const QSize& requested) override;
};
