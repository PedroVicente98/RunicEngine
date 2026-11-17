# Runic::Physics › Source/CollisionLayers

Collision layers and filters.

- **Filled by:** PHYS-1 (slice 7)
- **Status:** PHYS-1 (slice 7) wrote `CollisionLayers.cpp` (the table and the Engine's four layers) and `JoltLayers.hpp` (the table as Jolt's three layer interfaces)

Private to `Runic::Physics`: other modules reach this code only through the module's public headers. Files here use namespace `Runic::Physics` (or `Runic::Physics::Detail` for helpers several files of the module share); a helper used by one file sits in an unnamed namespace inside that file. Layout and rules: [the module's README](../../README.md).
