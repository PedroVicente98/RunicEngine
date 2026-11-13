# Runic::Network › Source/NetworkLayer

The Network layer: tick-safe ingress and egress queues and the connect-ticket hook, over the transport interface the Platform implements.

- **Filled by:** NET-1 (slice 12)
- **Status:** empty until NET-1 (slice 12) starts

Private to `Runic::Network`: other modules reach this code only through the module's public headers. Files here use namespace `Runic::Network` (or `Runic::Network::Detail` for helpers several files of the module share); a helper used by one file sits in an unnamed namespace inside that file. Layout and rules: [the module's README](../../README.md).
