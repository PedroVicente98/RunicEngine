# Runic::Core › Source/DebugStreams

The debug-stream registry where modules publish traces; the Diagnostics layer streams them to the RunicEditor's live attach.

- **Filled by:** CORE-2 (slice 3)
- **Status:** empty until CORE-2 (slice 3) starts

Private to `Runic::Core`: other modules reach this code only through the module's public headers. Files here use namespace `Runic::Core` (or `Runic::Core::Detail` for helpers several files of the module share); a helper used by one file sits in an unnamed namespace inside that file. Layout and rules: [the module's README](../../README.md).
