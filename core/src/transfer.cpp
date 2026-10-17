#include <quicky/transfer.hpp>
#include <algorithm>
#include <stdexcept>

namespace quicky {
Transfer::Transfer(const std::filesystem::path& source, std::size_t block_size) {
    if (block_size == 0 || block_size > 4 * 1024 * 1024)
        throw std::invalid_argument("Block size must be between 1 and 4194304 bytes");
    if (!std::filesystem::is_regular_file(source))
        throw std::invalid_argument("Source must be a regular file");
    progress_.total_bytes = std::filesystem::file_size(source);
    source_.open(source, std::ios::binary);
    if (!source_) throw std::runtime_error("Cannot open source file");
    buffer_.resize(block_size);
}
void Transfer::start() {
    if (progress_.state != TransferState::queued)
        throw std::logic_error("Only queued transfers can start");
    progress_.state = TransferState::transferring;
}
void Transfer::pause() {
    if (progress_.state != TransferState::transferring)
        throw std::logic_error("Only active transfers can pause");
    progress_.state = TransferState::paused;
}
void Transfer::resume() {
    if (progress_.state != TransferState::paused)
        throw std::logic_error("Only paused transfers can resume");
    progress_.state = TransferState::transferring;
}
void Transfer::cancel() {
    if (progress_.state == TransferState::completed ||
        progress_.state == TransferState::failed ||
        progress_.state == TransferState::cancelled) return;
    progress_.state = TransferState::cancelled;
    source_.close();
}
void Transfer::step(AuthenticatedChannel& channel) {
    if (progress_.state != TransferState::transferring)
        throw std::logic_error("Transfer is not active");
    try {
        const auto remaining = progress_.total_bytes - progress_.confirmed_bytes;
        if (remaining != 0) {
            const auto count = static_cast<std::size_t>(
                std::min<std::uint64_t>(remaining, buffer_.size()));
            source_.read(reinterpret_cast<char*>(buffer_.data()),
                         static_cast<std::streamsize>(count));
            if (static_cast<std::size_t>(source_.gcount()) != count)
                throw std::runtime_error("Source became unreadable or was truncated");
            channel.send_block(progress_.confirmed_bytes, {buffer_.data(), count});
            progress_.confirmed_bytes += count;
        }
        if (progress_.confirmed_bytes == progress_.total_bytes) {
            channel.finish(progress_.total_bytes);
            progress_.state = TransferState::completed;
            source_.close();
        }
    } catch (const std::exception& e) {
        progress_.state = TransferState::failed;
        progress_.error = e.what();
        source_.close();
    }
}
const char* state_name(TransferState state) noexcept {
    switch (state) {
    case TransferState::queued: return "queued";
    case TransferState::transferring: return "transferring";
    case TransferState::paused: return "paused";
    case TransferState::completed: return "completed";
    case TransferState::cancelled: return "cancelled";
    case TransferState::failed: return "failed";
    }
    return "unknown";
}
} // namespace quicky
