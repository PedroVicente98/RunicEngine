# Runic::Application › Source/Layers

Layer and LayerStack: layers and overlays as separate ranges owned through `unique_ptr`; push, pop and replace queued and applied at the frame's one safe point; each layer's time measured.

- **Filled by:** APP-1 (slice 1)
- **Status:** empty until APP-1 (slice 1) starts

Private to `Runic::Application`: other modules reach this code only through the module's public headers. Files here use namespace `Runic::Application` (or `Runic::Application::Detail` for helpers several files of the module share); a helper used by one file sits in an unnamed namespace inside that file. Layout and rules: [the module's README](../../README.md).
