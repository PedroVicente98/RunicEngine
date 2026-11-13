# Runic::ECS › Source/States

State declarations: one declaration per state, whose category picks its Flecs storage (hot flag, fact with a target, content flag, value, derived), with owner and observers.

- **Filled by:** ECS-2 (slice 4)
- **Status:** empty until ECS-2 (slice 4) starts

Private to `Runic::ECS`: other modules reach this code only through the module's public headers. Files here use namespace `Runic::ECS` (or `Runic::ECS::Detail` for helpers several files of the module share); a helper used by one file sits in an unnamed namespace inside that file. Layout and rules: [the module's README](../../README.md).
