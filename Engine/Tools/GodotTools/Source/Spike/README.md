# Runic::GodotTools › Source/Spike

The GDT-1 test plugin (slice 0): proves the RunicEditor loads Runic tool code compiled into it. Temporary: GDT-2 replaces it with the real plugin in `Plugin/` and deletes this folder.

- **Filled by:** GDT-1 (slice 0)
- **Status:** has code since slice 0; GDT-2 deletes this folder when the real plugin lands

Private to `Runic::GodotTools`: other modules reach this code only through the module's public headers. Files here use namespace `Runic::GodotTools` (or `Runic::GodotTools::Detail` for helpers several files of the module share); a helper used by one file sits in an unnamed namespace inside that file. Layout and rules: [the module's README](../../README.md).
