# Runic::Core › Source/RenderData

The code behind `Public/RenderData/` (namespace `Runic::RenderData`): the double-buffered RenderFrame, DebugGeometry and the debug-draw API that turns shapes into lines and triangles, then each render-data kind as the package that first needs it adds it (mesh, skinned, anchors; lights, environment, volumes; water and field grids; effects).

- **Filled by:** CORE-3 (slice 2), CORE-4 (slice 22), CORE-5 (slice 25a), CORE-6 (slice 42a), CORE-7 (slice 44+)
- **Status:** empty until CORE-3 (slice 2) starts

Private to `Runic::Core`: other modules reach this code only through the module's public headers. Files here use namespace `Runic::Core` (or `Runic::Core::Detail` for helpers several files of the module share); a helper used by one file sits in an unnamed namespace inside that file. Layout and rules: [the module's README](../../README.md).
