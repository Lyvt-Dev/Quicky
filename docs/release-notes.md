Quicky's first installable preview introduces a native Qt Quick interface and
real TLS 1.3 file transfer with receiver fingerprint pinning, temporary QR/link
invitations, explicit acceptance and SHA-256 verification.

Included: multi-file queue, pause/resume, recovery from retained partial files,
cancellation, local device discovery, text/link files, history and dark/light mode.

**Preview limitations**

- Same reachable IPv4 network; no Wi-Fi Direct, BLE, relay or mDNS yet.
- Pair through copied invitation links or the Android QR camera scanner.
- No persistent trusted-device pairing or contact-based visibility.
- Android must remain open and uses app-private receipt plus system Save export.
- Windows installer is unsigned; Android APK is debug-signed for preview testing.
- No production security audit, signed mobile-store release or iOS build yet.
- Interrupted transfers are resumed explicitly, not automatically retried.

Do not treat this preview as the completed cross-platform product. Native device
acceptance tests and production signing remain required before a stable release.
