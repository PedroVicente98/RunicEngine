# Runic::Core › Source/Logging

Logging hooks: a sink the host plugs in. Core decides no I/O; the host (or a test) chooses where lines go.

- **Filled by:** CORE-1 (slice 1)
- **Status:** empty until CORE-1 (slice 1) starts

Private to `Runic::Core`: other modules reach this code only through the module's public headers. Files here use namespace `Runic::Core` (or `Runic::Core::Detail` for helpers several files of the module share); a helper used by one file sits in an unnamed namespace inside that file. Layout and rules: [the module's README](../../README.md).
