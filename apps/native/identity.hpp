#pragma once
#include <QSslCertificate>
#include <QSslKey>
#include <QString>

struct Identity {
    QSslCertificate certificate;
    QSslKey privateKey;
    QString fingerprint;
    static Identity loadOrCreate(const QString& directory);
};
