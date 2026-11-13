# Runic::World › Source/SpatialIndex

The sparse 3D index (XZ columns holding only occupied Y cells), exact XYZ candidate queries (radius, box, ellipsoid) and membership updates.

- **Filled by:** WORLD-2 (slice 11)
- **Status:** empty until WORLD-2 (slice 11) starts

Private to `Runic::World`: other modules reach this code only through the module's public headers. Files here use namespace `Runic::World` (or `Runic::World::Detail` for helpers several files of the module share); a helper used by one file sits in an unnamed namespace inside that file. Layout and rules: [the module's README](../../README.md).
