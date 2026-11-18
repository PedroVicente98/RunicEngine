# Runic::World › Source/SpatialIndex

The sparse 3D index (XZ columns holding only occupied Y cells), exact XYZ candidate queries (radius, box, ellipsoid) and membership updates.

- **Filled by:** WORLD-2 (slice 11)
- **Status:** WORLD-2 (slice 11) wrote `SparseCells.hpp`/`.cpp` (the map of XZ columns, each holding only its occupied Y cells sorted by y, a cell's slots in the order they entered; a range walked column by column, or by the grid's own columns when it is wider), `SpatialIndex.cpp` (`CellsTouched`, the exact tests of `SpatialQuery`, membership and queries) and `SpatialDebug.cpp` (`DrawSpatialIndex`, `DrawSpatialQuery`)

Private to `Runic::World`: other modules reach this code only through the module's public headers. Files here use namespace `Runic::World` (or `Runic::World::Detail` for helpers several files of the module share); a helper used by one file sits in an unnamed namespace inside that file. Layout and rules: [the module's README](../../README.md).
