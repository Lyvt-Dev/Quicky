# Security model

## Implemented trust boundary

A receiving device owns a locally generated P-256 certificate and private key.
An out-of-band invitation conveys its SHA-256 certificate fingerprint and a
256-bit random bearer token. Invitations expire after five minutes, are never
broadcast, and may be renewed to invalidate previous links.

The sender requires TLS 1.3 and compares the certificate to the invitation.
Only pinned self-signed/hostname trust errors are waived. Certificate expiration
and unrelated TLS failures remain fatal. The receiver authorizes the bearer
token with a constant-time comparison and requires explicit acceptance per file.
This authenticates possession of an invitation, not the displayed sender name.
There is no plaintext network fallback.

Discovery is an unauthenticated UDP convenience hint. It does not establish
trust, distribute invitation secrets or initiate file receipt. All received file
names are checked against path traversal and Windows reserved names. Incoming
files use locked, hash-addressed temporary files. Finalization checks SHA-256,
avoids overwriting existing targets, and only then acknowledges success.

## Current limitations

The local network and discovery metadata are observable. A person who obtains
an invitation can request receipt until it expires, but still needs consent.
Invitations are multi-use within their validity period to permit file queues.
Device identities are permission-restricted files, not hardware-backed keys.
Windows relies on the user's profile ACLs. Root/administrator access is outside
the threat model. Never grant other users write access to the receive directory.

The sender's displayed progress is bytes queued to TLS; success requires the
receiver's final verified acknowledgement. Pause occurs by disconnecting; resume
rehashes the source and asks the receiver for its retained offset. Changes during
reading are detected by the final content hash. A resume with an already-corrupt
partial fails verification and removes that partial, requiring another attempt.

Network operations have timeouts and bounded buffers/headers/handshakes. Hashing
runs on workers, but there is no third-party security audit or fuzzing coverage.
No production-ready security claim is made. The preview does not implement
persistent mutual key trust, automatic retry, signed updates or mobile background
services. Pending incoming files require consent before allocating disk space.

## Production gates

Review the pairing/transfer protocol; add replay/rate-limit coverage, malicious
peer and filesystem race tests, platform credential-store protection, native
Windows/Android tests, stable Android signing and Windows Authenticode signing.
Validate interruption and low-storage behavior on real devices. Select a source
license and ship the exact third-party source/notices matching each release.
