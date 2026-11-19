# Runic::GodotBridge › Source/Input

The input sink: a node at the scene-tree root that receives only the input Godot's UI did not consume and turns it into AppEvents for the stack.

- **Filled by:** GDB-1 (slice 1)
- **Status:** has code since GDB-1 (`InputSink.hpp`, `InputSink.cpp`)

Private to `Runic::GodotBridge`: other modules reach this code only through the module's public headers. Files here use namespace `Runic::GodotBridge` (or `Runic::GodotBridge::Detail` for helpers several files of the module share); a helper used by one file sits in an unnamed namespace inside that file. Layout and rules: [the module's README](../../README.md).
