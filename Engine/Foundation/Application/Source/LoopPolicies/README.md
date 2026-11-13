# Runic::Application › Source/LoopPolicies

Loop policies: a fixed 60 Hz loop that sleeps until the next quantum (GameServer, RunicBot), and a loop paced by the top layer (GameClient and RunicEditor, where Godot's iteration waits for vsync or for the editor's low-processor mode).

- **Filled by:** APP-1 (slice 1)
- **Status:** empty until APP-1 (slice 1) starts

Private to `Runic::Application`: other modules reach this code only through the module's public headers. Files here use namespace `Runic::Application` (or `Runic::Application::Detail` for helpers several files of the module share); a helper used by one file sits in an unnamed namespace inside that file. Layout and rules: [the module's README](../../README.md).
