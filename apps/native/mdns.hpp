#pragma once
#include <QByteArray>
#include <QList>
#include <QString>
namespace mdns {
struct Advertisement { QString name, platform, fingerprint; quint16 port = 0; };
QByteArray announce(const Advertisement& device, const QString& ipv4);
QByteArray query();
QList<Advertisement> parse(const QByteArray& packet);
bool isQuery(const QByteArray& packet);
}
