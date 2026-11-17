# Runic::Physics › Source/DebugExtraction

Jolt's debug interface implemented here and written into Core DebugGeometry buffers, so Physics and GodotBridge never link each other.

- **Filled by:** PHYS-3 (slice 8)
- **Status:** PHYS-3 (slice 8) wrote `DebugCapture.hpp` and `DebugExtraction.cpp`: the capture behind Jolt's debug interface (into a frame's debug geometry relative to its origin, or a cached mesh in a shape's own space), `ExtractDebugGeometry`, and the descriptions tools read (`FindBodies`, `DescribeBody`, `DescribeShape`)

Private to `Runic::Physics`: other modules reach this code only through the module's public headers. Files here use namespace `Runic::Physics` (or `Runic::Physics::Detail` for helpers several files of the module share); a helper used by one file sits in an unnamed namespace inside that file. Layout and rules: [the module's README](../../README.md).
