# Runic::ECS › Source/Registration

Component, system and query registration through Runic types, and the conventions every module follows.

- **Filled by:** ECS-1 (slice 4)
- **Status:** ECS-1 (slice 4) wrote `ComponentTypes.cpp` and `ComponentNames.hpp`: the per-type indices and the `namespace:name` convention; a system is a function registered with the `SimulationRuntime` that runs cached queries, so there is no system registry here (ARCHITECTURE.md › Does not own)

Private to `Runic::ECS`: other modules reach this code only through the module's public headers. Files here use namespace `Runic::ECS` (or `Runic::ECS::Detail` for helpers several files of the module share); a helper used by one file sits in an unnamed namespace inside that file. Layout and rules: [the module's README](../../README.md).
