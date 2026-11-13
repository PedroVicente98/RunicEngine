# Runic::Events › Source/Readers

Readers and multi-consumer delivery: every consumer receives every fact, after the barrier that flushes it.

- **Filled by:** EVT-1 (slice 4)
- **Status:** empty until EVT-1 (slice 4) starts

Private to `Runic::Events`: other modules reach this code only through the module's public headers. Files here use namespace `Runic::Events` (or `Runic::Events::Detail` for helpers several files of the module share); a helper used by one file sits in an unnamed namespace inside that file. Layout and rules: [the module's README](../../README.md).
