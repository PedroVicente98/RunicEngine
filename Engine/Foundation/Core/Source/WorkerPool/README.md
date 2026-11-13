# Runic::Core › Source/WorkerPool

The worker pool every simulation runs its phases on; Jolt's job system is adapted onto it inside Physics (PHYS-4), so no library owns threads.

- **Filled by:** CORE-2 (slice 3)
- **Status:** empty until CORE-2 (slice 3) starts

Private to `Runic::Core`: other modules reach this code only through the module's public headers. Files here use namespace `Runic::Core` (or `Runic::Core::Detail` for helpers several files of the module share); a helper used by one file sits in an unnamed namespace inside that file. Layout and rules: [the module's README](../../README.md).
