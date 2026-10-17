#pragma once
#include <functional>
#include <string>
#include <vector>

namespace quicky {
struct Device {
    std::string discovery_id; // Untrusted discovery handle, never an identity proof.
    std::string display_name;
    std::string platform;
};
class Discovery {
public:
    virtual ~Discovery() = default;
    virtual void start(std::function<void(Device)> found) = 0;
    virtual void stop() = 0;
};
struct PairingInvitation {
    std::string qr_payload;
    std::string identity_fingerprint;
};
class Pairing {
public:
    virtual ~Pairing() = default;
    virtual PairingInvitation create_invitation() = 0;
    virtual void accept_invitation(std::string payload) = 0;
    virtual void revoke(std::string identity_fingerprint) = 0;
};
// Interfaces only: production implementations require native credential stores,
// invitation expiry/replay protection and a reviewed authentication protocol.
} // namespace quicky
