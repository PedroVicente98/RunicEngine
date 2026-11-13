# Runic::GodotBridge › Source/GodotLayer

The GodotLayer: the LibGodot instance inside the LayerStack, at the top of the client's stack; the window from the DisplayServer; headless mode; Godot's `iteration()` as the last step of each frame.

- **Filled by:** GDB-1 (slice 1)
- **Status:** empty until GDB-1 (slice 1) starts

Private to `Runic::GodotBridge`: other modules reach this code only through the module's public headers. Files here use namespace `Runic::GodotBridge` (or `Runic::GodotBridge::Detail` for helpers several files of the module share); a helper used by one file sits in an unnamed namespace inside that file. Layout and rules: [the module's README](../../README.md).
