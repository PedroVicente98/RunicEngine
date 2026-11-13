# Runic::AI › Source/Systems

The AI systems registered into the simulation: aggro, perception, think, target and sleep (perception lifecycle: warn and detect wake, target validation, leash, return home).

- **Filled by:** AI-1 (slice 32), AI-3 (slice 33), AI-4 (slice 34)
- **Status:** empty until AI-1 (slice 32) starts

Private to `Runic::AI`: other modules reach this code only through the module's public headers. Files here use namespace `Runic::AI` (or `Runic::AI::Detail` for helpers several files of the module share); a helper used by one file sits in an unnamed namespace inside that file. Layout and rules: [the module's README](../../README.md).
