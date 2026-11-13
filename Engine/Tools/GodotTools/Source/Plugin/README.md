# Runic::GodotTools › Source/Plugin

The real Runic editor plugin: what registers the Runic tools at the editor level and the editor's Runic menus. Its UI follows the editor theme and scale and uses only the editor's extension points.

- **Filled by:** GDT-2 (slice 6)
- **Status:** empty until GDT-2 (slice 6) starts

Private to `Runic::GodotTools`: other modules reach this code only through the module's public headers. Files here use namespace `Runic::GodotTools` (or `Runic::GodotTools::Detail` for helpers several files of the module share); a helper used by one file sits in an unnamed namespace inside that file. Layout and rules: [the module's README](../../README.md).
