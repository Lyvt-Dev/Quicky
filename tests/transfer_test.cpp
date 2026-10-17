#include <quicky/transfer.hpp>
#include <chrono>
#include <iostream>
#include <stdexcept>

void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
struct Receiver final : quicky::AuthenticatedChannel {
    std::vector<std::byte> data;
    bool reject = false;
    bool finished = false;
    bool reject_finish = false;
    void send_block(std::uint64_t offset, std::span<const std::byte> bytes) override {
        if (reject) throw std::runtime_error("Disconnected");
        check(offset == data.size(), "Non-contiguous block");
        data.insert(data.end(), bytes.begin(), bytes.end());
    }
    void finish(std::uint64_t size) override {
        if (reject_finish) throw std::runtime_error("Integrity check failed");
        check(data.size() == size, "Final size mismatch");
        finished = true;
    }
};
int main() {
    const auto directory = std::filesystem::temp_directory_path() /
        ("quicky-tests-" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directory(directory);
    struct Cleanup {
        std::filesystem::path path;
        ~Cleanup() { std::error_code ec; std::filesystem::remove_all(path, ec); }
    } cleanup{directory};
    try {
        const auto file = directory / "payload.bin";
        const std::string payload("abc\0defghij", 11);
        { std::ofstream out(file, std::ios::binary); out.write(payload.data(), payload.size()); }
        quicky::Transfer transfer(file, 4);
        Receiver receiver;
        transfer.start();
        transfer.step(receiver);
        check(transfer.progress().confirmed_bytes == 4, "First block progress");
        transfer.pause();
        bool rejected = false;
        try { transfer.step(receiver); } catch (const std::logic_error&) { rejected = true; }
        check(rejected, "Paused transfer must not send");
        transfer.resume();
        while (transfer.progress().state == quicky::TransferState::transferring)
            transfer.step(receiver);
        check(receiver.finished, "Receiver must finalize");
        check(receiver.data.size() == payload.size(), "Payload length");
        for (std::size_t i = 0; i < payload.size(); ++i)
            check(receiver.data[i] == static_cast<std::byte>(payload[i]), "Payload corruption");

        quicky::Transfer failed(file, 4);
        Receiver disconnected;
        disconnected.reject = true;
        failed.start(); failed.step(disconnected);
        check(failed.progress().state == quicky::TransferState::failed, "Disconnect state");
        check(failed.progress().confirmed_bytes == 0, "Unacknowledged bytes counted");
        check(!failed.progress().error.empty(), "Missing failure reason");

        quicky::Transfer cancelled(file);
        cancelled.start(); cancelled.cancel(); cancelled.cancel();
        check(cancelled.progress().state == quicky::TransferState::cancelled, "Cancellation");

        quicky::Transfer invalid_finish(file);
        Receiver bad_hash; bad_hash.reject_finish = true;
        invalid_finish.start(); invalid_finish.step(bad_hash);
        check(invalid_finish.progress().state == quicky::TransferState::failed,
              "Finalization failure must not report success");

        const auto empty = directory / "empty";
        { std::ofstream out(empty); }
        quicky::Transfer zero(empty);
        Receiver empty_receiver;
        zero.start(); zero.step(empty_receiver);
        check(zero.progress().state == quicky::TransferState::completed && empty_receiver.finished,
              "Empty file must finalize");

        rejected = false;
        try { quicky::Transfer invalid(file, 0); }
        catch (const std::invalid_argument&) { rejected = true; }
        check(rejected, "Zero block size accepted");
        rejected = false;
        try { quicky::Transfer missing(directory / "missing"); }
        catch (const std::exception&) { rejected = true; }
        check(rejected, "Missing source accepted");
        std::cout << "8 scenarios passed: binary streaming/pause/resume, disconnect, cancellation, "
                     "finalization failure, empty file, block bounds, missing source, exact bytes\n";
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
