# Runic::World › Source/Coordinates

Region-local origins and conversions on top of Core's WorldPosition (64-bit, real Y, 1 cm minimum unit).

- **Filled by:** WORLD-1 (slice 4)
- **Status:** WORLD-1 (slice 4) wrote `RegionFrame.cpp`: whether a place is within reach of a region's origin; the conversions themselves are inline in `Public/RegionFrame.hpp`, on Core's 64-bit operators

Private to `Runic::World`: other modules reach this code only through the module's public headers. Files here use namespace `Runic::World` (or `Runic::World::Detail` for helpers several files of the module share); a helper used by one file sits in an unnamed namespace inside that file. Layout and rules: [the module's README](../../README.md).
