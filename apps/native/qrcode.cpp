#include "qrcode.hpp"
#include "qrcodegen.hpp"
#include <QImage>
#include <QUrl>
QImage QrProvider::requestImage(const QString& id, QSize* size, const QSize&) {
    try {
        const auto text = QUrl::fromPercentEncoding(id.toUtf8()).toUtf8();
        const auto qr = qrcodegen::QrCode::encodeText(text.constData(), qrcodegen::QrCode::Ecc::MEDIUM);
        constexpr int scale = 5, border = 4;
        const int dimension = (qr.getSize() + border * 2) * scale;
        QImage image(dimension, dimension, QImage::Format_RGB32);
        image.fill(Qt::white);
        for (int y = 0; y < qr.getSize(); ++y)
            for (int x = 0; x < qr.getSize(); ++x)
                if (qr.getModule(x, y))
                    for (int dy = 0; dy < scale; ++dy)
                        for (int dx = 0; dx < scale; ++dx)
                            image.setPixelColor((x + border) * scale + dx, (y + border) * scale + dy, Qt::black);
        if (size) *size = image.size();
        return image;
    } catch (...) { return {}; }
}
