#include "identity.hpp"
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QSaveFile>
#include <openssl/pem.h>
#include <openssl/x509.h>
#include <openssl/rand.h>
#include <memory>
#include <stdexcept>

namespace {
using Bio = std::unique_ptr<BIO, decltype(&BIO_free)>;
QByteArray contents(BIO* bio) {
    char* data = nullptr;
    const auto size = BIO_get_mem_data(bio, &data);
    if (size <= 0) throw std::runtime_error("Cannot encode identity");
    return QByteArray(data, size);
}
void save(const QString& path, const QByteArray& bytes) {
    QSaveFile out(path);
    if (!out.open(QIODevice::WriteOnly) ||
        !out.setPermissions(QFile::ReadOwner | QFile::WriteOwner) ||
        out.write(bytes) != bytes.size() || !out.commit())
        throw std::runtime_error("Cannot securely save device identity");
}
QByteArray read(const QString& path) {
    QFile in(path);
    if (!in.open(QIODevice::ReadOnly)) throw std::runtime_error("Cannot read identity");
    return in.readAll();
}
}
Identity Identity::loadOrCreate(const QString& directory) {
    if (!QDir().mkpath(directory)) throw std::runtime_error("Cannot create identity directory");
    const auto certPath = directory + "/identity.pem";
    const auto keyPath = directory + "/identity-key.pem";
    if (QFile::exists(certPath) != QFile::exists(keyPath))
        throw std::runtime_error("Incomplete identity; refusing to silently replace trusted keys");
    if (!QFile::exists(keyPath)) {
        std::unique_ptr<EVP_PKEY_CTX, decltype(&EVP_PKEY_CTX_free)> ctx(
            EVP_PKEY_CTX_new_id(EVP_PKEY_EC, nullptr), EVP_PKEY_CTX_free);
        EVP_PKEY* rawKey = nullptr;
        if (!ctx || EVP_PKEY_keygen_init(ctx.get()) <= 0 ||
            EVP_PKEY_CTX_set_ec_paramgen_curve_nid(ctx.get(), NID_X9_62_prime256v1) <= 0 ||
            EVP_PKEY_keygen(ctx.get(), &rawKey) <= 0)
            throw std::runtime_error("Device key generation failed");
        std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)> key(rawKey, EVP_PKEY_free);
        std::unique_ptr<X509, decltype(&X509_free)> cert(X509_new(), X509_free);
        unsigned char randomSerial[16];
        if (!cert || RAND_bytes(randomSerial, sizeof randomSerial) != 1)
            throw std::runtime_error("Secure random generation failed");
        randomSerial[0] &= 0x7f;
        std::unique_ptr<BIGNUM, decltype(&BN_free)> serial(
            BN_bin2bn(randomSerial, sizeof randomSerial, nullptr), BN_free);
        if (!serial || !BN_to_ASN1_INTEGER(serial.get(), X509_get_serialNumber(cert.get())) ||
            !X509_set_version(cert.get(), 2) ||
            !X509_gmtime_adj(X509_getm_notBefore(cert.get()), -300) ||
            !X509_gmtime_adj(X509_getm_notAfter(cert.get()), 315360000L) ||
            !X509_set_pubkey(cert.get(), key.get()))
            throw std::runtime_error("Certificate setup failed");
        auto* name = X509_get_subject_name(cert.get());
        if (!X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC,
                reinterpret_cast<const unsigned char*>("Quicky device"), -1, -1, 0) ||
            !X509_set_issuer_name(cert.get(), name) ||
            !X509_sign(cert.get(), key.get(), EVP_sha256()))
            throw std::runtime_error("Certificate signing failed");
        Bio certBio(BIO_new(BIO_s_mem()), BIO_free);
        Bio keyBio(BIO_new(BIO_s_mem()), BIO_free);
        if (!certBio || !keyBio ||
            !PEM_write_bio_X509(certBio.get(), cert.get()) ||
            !PEM_write_bio_PrivateKey(keyBio.get(), key.get(), nullptr, nullptr, 0, nullptr, nullptr))
            throw std::runtime_error("Identity encoding failed");
        save(keyPath, contents(keyBio.get()));
        save(certPath, contents(certBio.get()));
    }
    Identity result;
    result.certificate = QSslCertificate(read(certPath));
    result.privateKey = QSslKey(read(keyPath), QSsl::Ec);
    if (result.certificate.isNull() || result.privateKey.isNull())
        throw std::runtime_error("Invalid device identity");
    // Reject mismatched keys rather than producing misleading pairing fingerprints.
    const auto certBytes = result.certificate.toPem();
    const auto keyBytes = read(keyPath);
    Bio certBio(BIO_new_mem_buf(certBytes.constData(), certBytes.size()), BIO_free);
    Bio keyBio(BIO_new_mem_buf(keyBytes.constData(), keyBytes.size()), BIO_free);
    std::unique_ptr<X509, decltype(&X509_free)> cert(
        PEM_read_bio_X509(certBio.get(), nullptr, nullptr, nullptr), X509_free);
    std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)> key(
        PEM_read_bio_PrivateKey(keyBio.get(), nullptr, nullptr, nullptr), EVP_PKEY_free);
    if (!cert || !key || X509_check_private_key(cert.get(), key.get()) != 1)
        throw std::runtime_error("Device certificate does not match private key");
    result.fingerprint = QString::fromLatin1(result.certificate.digest(QCryptographicHash::Sha256).toHex());
    return result;
}
