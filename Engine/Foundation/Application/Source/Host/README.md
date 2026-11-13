# Runic::Application › Source/Host

ApplicationHost: `Run()` repeats `RunFrame()` under the loop policy, and `RunFrame()` is one complete frame (drain queued events, update bottom-up, render bottom-up, apply layer changes), so a host that does not own the loop can still drive one frame at a time. Stop requests.

- **Filled by:** APP-1 (slice 1)
- **Status:** empty until APP-1 (slice 1) starts

Private to `Runic::Application`: other modules reach this code only through the module's public headers. Files here use namespace `Runic::Application` (or `Runic::Application::Detail` for helpers several files of the module share); a helper used by one file sits in an unnamed namespace inside that file. Layout and rules: [the module's README](../../README.md).
