# Runic::GodotTools › Source/Export

Cooker integration: the export plugin that calls the Cooker, collision presets in Godot's import settings, the collision naming convention for glTF.

- **Filled by:** GDT-4 (slice 9)
- **Status:** GDT-4 (slice 9) wrote `SceneReader.cpp` (a scene Godot imported as the Cooker's input: assets, placements, the naming convention, colliders), `EditorCook.cpp` (the cook, and loading it into the local world) and `RunicImportPlugin` (the import plugin's collision preset, the export plugin that cooks); the API is `Public/EditorCook.hpp`

Private to `Runic::GodotTools`: other modules reach this code only through the module's public headers. Files here use namespace `Runic::GodotTools` (or `Runic::GodotTools::Detail` for helpers several files of the module share); a helper used by one file sits in an unnamed namespace inside that file. Layout and rules: [the module's README](../../README.md).
