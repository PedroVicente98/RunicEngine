# Runic::GodotTools › Source/Project

The project model: runic.project.json, New and Open (per the open decision), and loading and reloading the project's editor library. Where the runic-mmo template's files live in this repository is still to decide.

- **Filled by:** GDT-2 (slice 6)
- **Status:** GDT-2 (slice 6) wrote `RunicProject.cpp` (the manifest, Open, New), `Json.hpp`/`Json.cpp` (a strict JSON reader and writer), `GameLibrary.hpp`/`GameLibrary.cpp` (loading and reloading the project's editor library) and `Template/runic-mmo/` (the new-project template, staged in `<build>/templates/`)

Private to `Runic::GodotTools`: other modules reach this code only through the module's public headers. Files here use namespace `Runic::GodotTools` (or `Runic::GodotTools::Detail` for helpers several files of the module share); a helper used by one file sits in an unnamed namespace inside that file. Layout and rules: [the module's README](../../README.md).
