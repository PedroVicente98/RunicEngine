# Runic::Cooker › Source/Inputs

What the Cooker reads: geometry and placements from the scenes Godot imported, handed over as plain data by GodotTools' export plugin (the Cooker links no Godot and has no second glTF importer).

- **Filled by:** COOK-1 (slice 9)
- **Status:** COOK-1 (slice 9) wrote `InputChecks.hpp`/`.cpp`: every problem in what GodotTools hands over, named by scene or asset source and node; the input types are `Public/CookInput.hpp`

Private to `Runic::Cooker`: other modules reach this code only through the module's public headers. Files here use namespace `Runic::Cooker` (or `Runic::Cooker::Detail` for helpers several files of the module share); a helper used by one file sits in an unnamed namespace inside that file. Layout and rules: [the module's README](../../README.md).
