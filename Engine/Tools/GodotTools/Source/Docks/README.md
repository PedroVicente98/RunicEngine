# Runic::GodotTools › Source/Docks

Docks: world, ECS and physics inspectors over a local simulation in the editor process, and the resolver debugger. Docks take their colors, fonts, icons and sizes from the editor theme and the editor scale.

- **Filled by:** GDT-2 (slice 6), GDT-3 (slice 8), GDT-5 (slice 17)
- **Status:** GDT-2 (slice 6) wrote `WorldInspector.cpp` (the inspectors' model) and `RunicWorldDock` (the world and ECS dock)

Private to `Runic::GodotTools`: other modules reach this code only through the module's public headers. Files here use namespace `Runic::GodotTools` (or `Runic::GodotTools::Detail` for helpers several files of the module share); a helper used by one file sits in an unnamed namespace inside that file. Layout and rules: [the module's README](../../README.md).
