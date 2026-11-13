# Runic::Environment › Source/Clock

The environment clock: world time, day and night phase, sun direction and the skylight input Perception reads.

- **Filled by:** ENV-1 (slice 25)
- **Status:** empty until ENV-1 (slice 25) starts

Private to `Runic::Environment`: other modules reach this code only through the module's public headers. Files here use namespace `Runic::Environment` (or `Runic::Environment::Detail` for helpers several files of the module share); a helper used by one file sits in an unnamed namespace inside that file. Layout and rules: [the module's README](../../README.md).
