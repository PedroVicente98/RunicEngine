# Runic::Core › Source/RenderData

The code behind `Public/RenderData/` (namespace `Runic::RenderData`): the double-buffered RenderFrame, DebugGeometry and the debug-draw API that turns shapes into lines and triangles, then each render-data kind as the package that first needs it adds it (mesh, skinned, anchors; lights, environment, volumes; water and field grids; effects).

- **Filled by:** CORE-3 (slice 2), CORE-4 (slice 22), CORE-5 (slice 25a), CORE-6 (slice 42a), CORE-7 (slice 44+)
- **Status:** CORE-3 (slice 2): `DebugGeometry.cpp`, `DebugDraw.cpp` (the shapes), `RenderFrameExchange.cpp`; slice 5: `ShapeInstances.cpp` (the shape-instance writer); slice 8: `DebugMeshes.cpp` (the cached debug meshes' library) and mesh instances in `DebugGeometry.cpp`

Private to `Runic::Core`: other modules reach this code only through the module's public headers. Files here use namespace `Runic::RenderData` (or `Runic::RenderData::Detail` for helpers several files share); a helper used by one file sits in an unnamed namespace inside that file. Layout and rules: [the module's README](../../README.md).
