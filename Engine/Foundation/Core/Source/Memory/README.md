# Runic::Core › Source/Memory

Memory utilities: fixed-capacity containers for allocation-free paths (ring buffers, small vectors; CORE-1, which APP-1's allocation-free events need) and the per-tick arenas and per-worker buffers the simulation phases write into (CORE-2).

- **Filled by:** CORE-1 (slice 1), CORE-2 (slice 3)
- **Status:** empty until CORE-1 (slice 1) starts

Private to `Runic::Core`: other modules reach this code only through the module's public headers. Files here use namespace `Runic::Core` (or `Runic::Core::Detail` for helpers several files of the module share); a helper used by one file sits in an unnamed namespace inside that file. Layout and rules: [the module's README](../../README.md).
