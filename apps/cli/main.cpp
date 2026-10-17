#include <quicky/transfer.hpp>
#include <iostream>

// Deliberately local-only: exercises streaming without pretending to provide
// encryption, pairing or a functioning network transport.
class LocalDryRun final : public quicky::AuthenticatedChannel {
public:
    void send_block(std::uint64_t, std::span<const std::byte> bytes) override {
        consumed_ += bytes.size();
    }
    void finish(std::uint64_t size) override {
        if (consumed_ != size) throw std::runtime_error("Byte count mismatch");
    }
private:
    std::uint64_t consumed_ = 0;
};
int main(int argc, char** argv) {
    if (argc != 3 || std::string(argv[1]) != "dry-run") {
        std::cout << "Quicky core prototype\nUsage: quicky dry-run <file>\n"
                     "Local streaming check only. No network transfer.\n";
        return argc == 1 ? 0 : 2;
    }
    try {
        quicky::Transfer transfer(argv[2]);
        LocalDryRun channel;
        transfer.start();
        while (transfer.progress().state == quicky::TransferState::transferring)
            transfer.step(channel);
        const auto& p = transfer.progress();
        std::cout << quicky::state_name(p.state) << ": " << p.confirmed_bytes
                  << "/" << p.total_bytes << " bytes locally consumed\n";
        if (!p.error.empty()) std::cerr << p.error << '\n';
        return p.state == quicky::TransferState::completed ? 0 : 1;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
