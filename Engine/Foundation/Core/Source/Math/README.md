# Runic::Core › Source/Math

The math vocabulary: GLM types (the approved public exception), and `Public/WorldSpace.hpp` (WorldPosition and WorldDelta in 64-bit double, ToLocal to 32-bit). The type rule: points and differences in the world are 64-bit; directions, sizes, rotations and local offsets are 32-bit; float-conversion warnings are errors.

- **Filled by:** CORE-1 (slice 1)
- **Status:** empty until CORE-1 (slice 1) starts

Private to `Runic::Core`: other modules reach this code only through the module's public headers. Files here use namespace `Runic::Core` (or `Runic::Core::Detail` for helpers several files of the module share); a helper used by one file sits in an unnamed namespace inside that file. Layout and rules: [the module's README](../../README.md).
