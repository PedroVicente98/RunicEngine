# Runic::Physics › Source/PhysicsWorld

PhysicsWorld, bodies and shapes on Jolt 5.6 in double precision, behind Runic types. Jolt has no native door: it stays private to this module.

- **Filled by:** PHYS-1 (slice 7)
- **Status:** PHYS-1 (slice 7) wrote `PhysicsWorld.cpp` (shapes, bodies in batches, the step, contacts as facts), `WorldState.hpp` (the state the module's files share, `PhysicsWorld::State`; PHYS-2 added the characters to it, PHYS-3 the debug view's capture; a registered shape carries its `ShapeId` in Jolt's user data) and `JoltRuntime` (Jolt's process-wide setup)

Private to `Runic::Physics`: other modules reach this code only through the module's public headers. Files here use namespace `Runic::Physics` (or `Runic::Physics::Detail` for helpers several files of the module share); a helper used by one file sits in an unnamed namespace inside that file. Layout and rules: [the module's README](../../README.md).
