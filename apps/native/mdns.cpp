#include "mdns.hpp"
#include <QMap>
#include <QHostAddress>
#include <QRegularExpression>
#include <optional>
namespace mdns {
namespace {
const QByteArray service = "_quicky._tcp.local";
void u16(QByteArray& out, quint16 n) { out += char(n >> 8); out += char(n & 255); }
void u32(QByteArray& out, quint32 n) { u16(out, quint16(n >> 16)); u16(out, quint16(n)); }
QByteArray name(const QByteArray& value) {
    QByteArray out;
    for (const auto& part : value.split('.')) { out += char(part.size()); out += part; }
    out += '\0'; return out;
}
quint16 get16(const QByteArray& data, qsizetype offset) {
    return (quint16(quint8(data[offset])) << 8) | quint8(data[offset + 1]);
}
std::optional<QByteArray> readName(const QByteArray& data, qsizetype& cursor) {
    QByteArray result; qsizetype pos = cursor; bool jumped = false;
    for (int hops = 0; hops < 128; ++hops) {
        if (pos < 0 || pos >= data.size()) return {};
        const auto length = quint8(data[pos++]);
        if (!length) { if (!jumped) cursor = pos; return result.toLower(); }
        if ((length & 0xc0) == 0xc0) {
            if (pos >= data.size()) return {};
            const auto target = ((length & 0x3f) << 8) | quint8(data[pos++]);
            if (!jumped) cursor = pos;
            jumped = true; pos = target; continue;
        }
        if (length > 63 || pos + length > data.size() || result.size() + length > 253) return {};
        if (!result.isEmpty()) result += '.';
        result += data.mid(pos, length); pos += length;
    }
    return {};
}
void record(QByteArray& out, const QByteArray& owner, quint16 type, const QByteArray& bytes, bool unique = true) {
    out += name(owner); u16(out, type); u16(out, unique ? 0x8001 : 1);
    u32(out, 15); u16(out, quint16(bytes.size())); out += bytes;
}
QByteArray txt(const QByteArray& value) { return QByteArray(1, char(value.size())) + value; }
}
QByteArray query() {
    QByteArray out(12, '\0'); out[5] = 1;
    out += name(service); u16(out, 12); u16(out, 1); return out;
}
QByteArray announce(const Advertisement& d, const QString& ipv4) {
    QByteArray out(12, '\0'); out[2] = char(0x84); out[7] = 4;
    const auto instance = "q-" + d.fingerprint.left(16).toLatin1() + "." + service;
    const auto host = "q-" + d.fingerprint.left(16).toLatin1() + ".local";
    record(out, service, 12, name(instance), false);
    QByteArray srv(4, '\0'); u16(srv, d.port); srv += name(host);
    record(out, instance, 33, srv);
    record(out, instance, 16, txt("name=" + d.name.left(60).toUtf8()) +
        txt("platform=" + d.platform.left(30).toUtf8()) + txt("fp=" + d.fingerprint.toLatin1()));
    QByteArray address; u32(address, QHostAddress(ipv4).toIPv4Address());
    record(out, host, 1, address); return out;
}
bool isQuery(const QByteArray& data) {
    if (data.size() < 12 || (quint8(data[2]) & 0x80) || get16(data, 4) > 32) return false;
    qsizetype pos = 12;
    for (int i = 0; i < get16(data, 4); ++i) {
        const auto n = readName(data, pos);
        if (!n || pos + 4 > data.size()) return false;
        if (*n == service && (get16(data, pos) == 12 || get16(data, pos) == 255)) return true;
        pos += 4;
    }
    return false;
}
QList<Advertisement> parse(const QByteArray& data) {
    if (data.size() < 12 || data.size() > 9000 || !(quint8(data[2]) & 0x80)) return {};
    const auto questions = get16(data, 4);
    const auto records = int(get16(data, 6)) + get16(data, 8) + get16(data, 10);
    if (questions > 32 || records > 128) return {};
    qsizetype pos = 12;
    for (int i = 0; i < questions; ++i) {
        if (!readName(data, pos) || pos + 4 > data.size()) return {};
        pos += 4;
    }
    QMap<QByteArray, Advertisement> found;
    for (int i = 0; i < records; ++i) {
        const auto owner = readName(data, pos);
        if (!owner || pos + 10 > data.size()) return {};
        const auto type = get16(data, pos), length = get16(data, pos + 8);
        pos += 10;
        if (pos + length > data.size()) return {};
        const auto end = pos + length;
        if (owner->endsWith("." + service)) {
            auto& device = found[*owner];
            if (type == 33 && length >= 7) device.port = get16(data, pos + 4);
            if (type == 16) {
                auto cursor = pos;
                while (cursor < end) {
                    const auto count = quint8(data[cursor++]);
                    if (cursor + count > end) return {};
                    const auto entry = data.mid(cursor, count); cursor += count;
                    if (entry.startsWith("name=")) device.name = QString::fromUtf8(entry.mid(5)).left(60);
                    if (entry.startsWith("platform=")) device.platform = QString::fromUtf8(entry.mid(9)).left(30);
                    if (entry.startsWith("fp=")) device.fingerprint = QString::fromLatin1(entry.mid(3));
                }
            }
        }
        pos = end;
    }
    QList<Advertisement> result;
    static const QRegularExpression fp("^[a-f0-9]{64}$");
    for (const auto& device : found)
        if (device.port && !device.name.isEmpty() && fp.match(device.fingerprint).hasMatch()) result += device;
    return result;
}
}
