# Runic::ECS › Source/FlecsWorld

The Flecs world wrapper (Flecs 4.2). Flecs is private to this module.

- **Filled by:** ECS-1 (slice 4)
- **Status:** ECS-1 (slice 4) wrote `EntityWorld.cpp`: the world on Flecs 4.2 (entities, component storage, cached queries, held-back changes on the main stage, the tools' listings)

Private to `Runic::ECS`: other modules reach this code only through the module's public headers. Files here use namespace `Runic::ECS` (or `Runic::ECS::Detail` for helpers several files of the module share); a helper used by one file sits in an unnamed namespace inside that file. Layout and rules: [the module's README](../../README.md).
