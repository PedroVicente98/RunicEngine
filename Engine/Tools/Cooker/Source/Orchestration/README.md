# Runic::Cooker › Source/Orchestration

Cook jobs: what to cook, progress, incremental cooking of only what changed.

- **Filled by:** COOK-1 (slice 9), COOK-8 (slice 44+)
- **Status:** COOK-1 (slice 9) wrote `Cooker.cpp`: a full cook (check everything, build every file in memory, then replace the previous cook), the stable IDs, the report

Private to `Runic::Cooker`: other modules reach this code only through the module's public headers. Files here use namespace `Runic::Cooker` (or `Runic::Cooker::Detail` for helpers several files of the module share); a helper used by one file sits in an unnamed namespace inside that file. Layout and rules: [the module's README](../../README.md).
