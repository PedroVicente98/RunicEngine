# Runic::Events › Source/Buffers

Typed, tick-safe event buffers that systems write facts into during a phase (HitEvent, DamageEvent, EntityKilled…).

- **Filled by:** EVT-1 (slice 4)
- **Status:** EVT-1 (slice 4) wrote `EventHub.cpp`: the hub that flushes a simulation's buffers in a fixed order, and a buffer's registration and description; `EventBuffer<T>` itself is a template in `Public/EventBuffer.hpp`

Private to `Runic::Events`: other modules reach this code only through the module's public headers. Files here use namespace `Runic::Events` (or `Runic::Events::Detail` for helpers several files of the module share); a helper used by one file sits in an unnamed namespace inside that file. Layout and rules: [the module's README](../../README.md).
