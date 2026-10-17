# Quicky

Native C++20 / Qt Quick app for encrypted local file sharing. Windows and Android
are the initial targets; the desktop app also builds on Linux and macOS.

## Implemented

- Responsive dark/light native Qt Quick interface with device list, settings and history.
- TLS 1.3, receiver certificate pinned to the fingerprint in a QR invitation.
- Cryptographically random, five-minute invitation; explicit consent for each file.
- Streaming file transfer with bounded buffers and SHA-256 content verification.
- Multi-file queue, cancellation, sender pause/resume and retained partial-file recovery.
- Local device discovery via UDP broadcast (not yet mDNS/BLE).
- Text and links sent as UTF-8 files.
- Collision-safe received filenames, disk-space checks and path validation.
- Windows per-user NSIS installer and automated install/run/uninstall smoke tests in CI.
- Android document picker and native export through the system Save dialog.

## Use

1. Connect both devices to the same reachable network and open Quicky.
2. On the receiving device, copy its five-minute QR invitation.
3. Paste the invitation into the sender, choose files and send.
4. Confirm each incoming file on the receiver.
5. On Android, use **Empfangene Datei speichern** to export the received file
   from app-private storage to a selected system location.

The QR code encodes the same invitation. In-app camera scanning is not yet
implemented; use the copy/paste link workflow. Discovery names are untrusted.
Never publish invitations. Open Quicky on both devices during transfers.

## Build (desktop)

Requirements: CMake 3.25+, C++20 compiler, Qt 6.4+ with Quick, QuickControls2,
Network and Concurrent, and OpenSSL 3 development libraries.

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
./build/Quicky
```

Set `CMAKE_PREFIX_PATH` to your Qt installation if it is not installed system-wide.
Set `OPENSSL_ROOT_DIR` when OpenSSL is installed outside the normal search path.
On Windows, use an MSVC developer shell and see
[installer build](packaging/windows/build.ps1).
For Android see [build script](packaging/android/build.sh).

Core-only build (no Qt): `cmake -S . -B build-core -DQUICKY_BUILD_APP=OFF`.
The diagnostic CLI is `build/quicky-cli dry-run README.md`; it sends nothing.

## Release status

This is a **preview**, not a security-audited production release. Local Linux
integration tests verify actual encrypted sockets, byte equality, invitation
rejection, refusal, resume, collision handling, integrity failure and queues.
Windows and Android builds are validated separately by CI; do not equate a Linux
pass with native device validation.

Tagging `v*` starts the release workflow. Publication is gated on Linux, Windows
and Android jobs passing. Windows installers are unsigned, and Android preview
APKs use a debug signing key. Production signing is a separate owner-managed step.
See [release notes](docs/release-notes.md) and [security](docs/security.md).

## Repository

- `core/`: small standard-C++ transfer state machine and service contracts.
- `apps/native/`: functional Qt network engine, certificate identity and QML UI.
- `tests/`: core and TLS integration tests.
- `packaging/`: Windows installer, Android project and Linux package builder.
- `.github/workflows/`: cross-platform builds, tests and release publication.
- `third_party/`, `licenses/`: QR generator and dependency notices.

No analytics, account or cloud storage is used. Identity and history are stored
locally. The own-source license remains for the repository owner to select.
