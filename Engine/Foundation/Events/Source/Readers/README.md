# Runic::Events › Source/Readers

Readers and multi-consumer delivery: every consumer receives every fact, after the barrier that flushes it.

- **Filled by:** EVT-1 (slice 4)
- **Status:** EVT-1 (slice 4) wrote `ReaderCursors.cpp`: the readers' cursors, and what a flush lets go (events every reader has read, and the oldest of a buffer over its capacity); `EventReader<T>` itself is a template in `Public/EventBuffer.hpp`

Private to `Runic::Events`: other modules reach this code only through the module's public headers. Files here use namespace `Runic::Events` (or `Runic::Events::Detail` for helpers several files of the module share); a helper used by one file sits in an unnamed namespace inside that file. Layout and rules: [the module's README](../../README.md).
