# Runic::GodotBridge › Source/LibGodot

The LibGodot host: `GodotRuntime` (load libgodot, create the one instance with the command line, start, iterate, destroy; class-registration hooks by initialization level) and `SharedLibrary`, the loader that opens libgodot itself (not GDExtensions).

- **Filled by:** slice 0: GodotBridge's first code, the LibGodot host written for the GDT-1 spike (BUILD-1 decision 12); GDB-1 (slice 1) builds the GodotLayer on it
- **Status:** has code since slice 0

Private to `Runic::GodotBridge`: other modules reach this code only through the module's public headers. Files here use namespace `Runic::GodotBridge` (or `Runic::GodotBridge::Detail` for helpers several files of the module share); a helper used by one file sits in an unnamed namespace inside that file. Layout and rules: [the module's README](../../README.md).
