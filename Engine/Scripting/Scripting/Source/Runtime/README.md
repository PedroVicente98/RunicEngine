# Runic::Scripting › Source/Runtime

One Lua VM per simulation on the simulation thread: the sandboxed environment, the memory cap per VM, the instruction budget per call, errors isolated per entry.

- **Filled by:** SCR-1 (slice 37a)
- **Status:** empty until SCR-1 (slice 37a) starts

Private to `Runic::Scripting`: other modules reach this code only through the module's public headers. Files here use namespace `Runic::Scripting` (or `Runic::Scripting::Detail` for helpers several files of the module share); a helper used by one file sits in an unnamed namespace inside that file. Layout and rules: [the module's README](../../README.md).
