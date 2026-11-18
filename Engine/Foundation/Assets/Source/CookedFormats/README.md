# Runic::Assets › Source/CookedFormats

Loading the native cooked formats (meshes, collision); the Cooker links each runtime module's writer.

- **Filled by:** ASSET-1 (slice 9)
- **Status:** ASSET-1 (slice 9) wrote `CookedFormats.cpp` (the header, the mesh, collision and cell payloads, their checks, whole-file writes) and `ByteStream.hpp` (little-endian bytes)

Private to `Runic::Assets`: other modules reach this code only through the module's public headers. Files here use namespace `Runic::Assets` (or `Runic::Assets::Detail` for helpers several files of the module share); a helper used by one file sits in an unnamed namespace inside that file. Layout and rules: [the module's README](../../README.md).
