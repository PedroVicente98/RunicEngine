# Runic::Physics › Source/JobSystem

A Jolt JobSystem adapter on the Core worker pool, so Jolt never owns threads.

- **Filled by:** PHYS-4 (slice 10)
- **Status:** PHYS-4 (slice 10) wrote `PoolJobSystem.hpp` and `PoolJobSystem.cpp`: Jolt's `JobSystemWithBarrier` with its jobs in a fixed free list, each ready job handed to the Core worker pool as a Core job (nothing queued without a pool or with one worker: Jolt's barrier runs every job on the stepping thread), and the drain that leaves no job behind a step

Private to `Runic::Physics`: other modules reach this code only through the module's public headers. Files here use namespace `Runic::Physics` (or `Runic::Physics::Detail` for helpers several files of the module share); a helper used by one file sits in an unnamed namespace inside that file. Layout and rules: [the module's README](../../README.md).
