#pragma once
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <span>
#include <string>
#include <vector>

namespace quicky {
enum class TransferState { queued, transferring, paused, completed, cancelled, failed };
struct Progress {
    TransferState state = TransferState::queued;
    std::uint64_t confirmed_bytes = 0;
    std::uint64_t total_bytes = 0;
    std::string error;
};

// Implementations must provide end-to-end authenticated encryption before
// exposing a channel here. No raw TCP or plaintext network implementation.
class AuthenticatedChannel {
public:
    virtual ~AuthenticatedChannel() = default;
    // Returns only after the peer has durably acknowledged the complete block.
    // The transport owns deadlines, cancellation and bounded framing.
    virtual void send_block(std::uint64_t offset,
                            std::span<const std::byte> bytes) = 0;
    // The receiver verifies the manifest/content and finalizes its temporary file.
    virtual void finish(std::uint64_t size) = 0;
};

// Single-threaded pump: the owner serializes all calls. Each step sends at most
// one block; UI adapters must run blocking I/O off their UI thread.
class Transfer {
public:
    explicit Transfer(const std::filesystem::path& source,
                      std::size_t block_size = 256 * 1024);
    const Progress& progress() const noexcept { return progress_; }
    void start();
    void pause();
    void resume();
    void cancel();
    void step(AuthenticatedChannel& channel);
private:
    std::ifstream source_;
    std::vector<std::byte> buffer_;
    Progress progress_;
};
const char* state_name(TransferState state) noexcept;
} // namespace quicky
