# Runic::Content › Source/Registries

Registries and lookup; duplicate and unknown IDs reported with their source. The same registration later fills the editor's palettes and the Cooker's checks.

- **Filled by:** CNT-1 (slice 9)
- **Status:** CNT-1 (slice 9) wrote `ContentRegistry.cpp`: sources and the problems' sentences (the registry itself is the template in `Public/ContentRegistry.hpp`)

Private to `Runic::Content`: other modules reach this code only through the module's public headers. Files here use namespace `Runic::Content` (or `Runic::Content::Detail` for helpers several files of the module share); a helper used by one file sits in an unnamed namespace inside that file. Layout and rules: [the module's README](../../README.md).
