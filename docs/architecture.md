# Architecture

The implemented native app uses Qt Quick/QML on desktop and Android. Both call
`Engine`, an event-driven C++20 service using Qt Network TLS sockets, a bounded
file pump, QtConcurrent hashing and local settings/history. OpenSSL generates
the device identity; Qt handles TLS 1.3. `QrProvider` renders the QR invitation
with Nayuki's MIT-licensed generator.

The portable `core/` library remains independent of Qt and models single-file
transfer state through a channel contract. The network engine currently owns its
own asynchronous transfer state; it is not yet an adapter of the portable pump.
This separation is explicit rather than claiming JNI/Swift integrations exist.

## Wire format v1

1. TLS 1.3 handshake; sender verifies the exact receiver certificate fingerprint.
2. Sender sends one UTF-8 JSON line (maximum 16 KiB): `v:1`, `type:offer`,
   `token`, safe `name`, decimal string `size`, SHA-256 hex `sha256`, `sender`.
3. Receiver validates the invitation and requests local user consent.
4. Receiver replies `type:ready`, decimal string `offset` for an existing partial.
5. Sender streams exactly `size-offset` binary bytes, without JSON framing.
6. Receiver hashes the complete temporary file, finalizes it, then responds
   `type:done`, `sha256`. A failure response is `type:error`, `message`.
7. One file per connection. A sender queue repeats these steps per file.

The receiver never derives paths from a peer-supplied directory. Files are staged
under `.quicky-partials/<sha256>.part`. A per-file lock prevents duplicate active
writers; completed filenames receive a random suffix when a target exists.

Discovery currently broadcasts compact JSON on UDP 45439; a TLS listener chooses
an available TCP port. No bearer token is included in announcements. Devices
expire from the UI after 12 seconds without an announcement.

## Next platform work

mDNS/NSD adapters, authenticated persistent device trust, QR camera scanning,
Android foreground service, iOS lifecycle/share integration, automatic retry,
folder manifests, OS credential stores and signed distribution are still needed
for the complete product vision. See `security.md` for release gates.
