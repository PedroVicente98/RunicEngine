# Runic::World › Source/Volumes

Authored volumes (caves, rule areas, indivisible volumes) with point-in-volume queries; the volume kind picks the backend.

- **Filled by:** WORLD-2 (slice 11)
- **Status:** WORLD-2 (slice 11) wrote `VolumeShapes.hpp` (the parts' geometry in 64-bit: half sizes, the point test, the box around turned parts), `AuthoredVolumes.cpp` (kinds and their backends, volumes with generation-tagged IDs, the rule areas in a `SparseCells` of their own, the exact point test, the volumes that need a Physics body) and `VolumeDebug.cpp` (`DrawVolumes`)

Private to `Runic::World`: other modules reach this code only through the module's public headers. Files here use namespace `Runic::World` (or `Runic::World::Detail` for helpers several files of the module share); a helper used by one file sits in an unnamed namespace inside that file. Layout and rules: [the module's README](../../README.md).
