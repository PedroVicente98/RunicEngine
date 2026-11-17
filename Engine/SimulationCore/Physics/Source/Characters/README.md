# Runic::Physics › Source/Characters

CharacterVirtual integration and ground movement.

- **Filled by:** PHYS-2 (slice 7)
- **Status:** PHYS-2 (slice 7) wrote `CharacterSet.hpp` (character slots and handles, shared capsules, the column grid that finds characters near each other, the listener that keeps them from pushing each other) and `Characters.cpp` (the PhysicsWorld's character methods and the update before the bodies step)

Private to `Runic::Physics`: other modules reach this code only through the module's public headers. Files here use namespace `Runic::Physics` (or `Runic::Physics::Detail` for helpers several files of the module share); a helper used by one file sits in an unnamed namespace inside that file. Layout and rules: [the module's README](../../README.md).
