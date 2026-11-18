# Runic::Cooker › Source/Collision

Jolt collision cooking, meshoptimizer for collision meshes.

- **Filled by:** COOK-1 (slice 9)
- **Status:** COOK-1 (slice 9) wrote `CollisionCooking.hpp`/`.cpp`: triangles scaled per placement, welded to a 0.1 mm grid, cleaned, proven buildable by Jolt through Runic::Physics; the visual mesh merged with its normals. meshoptimizer is not used yet

Private to `Runic::Cooker`: other modules reach this code only through the module's public headers. Files here use namespace `Runic::Cooker` (or `Runic::Cooker::Detail` for helpers several files of the module share); a helper used by one file sits in an unnamed namespace inside that file. Layout and rules: [the module's README](../../README.md).
