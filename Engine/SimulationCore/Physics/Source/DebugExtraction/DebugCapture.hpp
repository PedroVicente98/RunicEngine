#pragma once

#include <Jolt/Jolt.h>

#include <memory>

#include "RenderData/DebugGeometry.hpp"
#include "RenderData/DebugMeshes.hpp"

// The adapter between Jolt's debug interface and Core's debug geometry (PHYS-3; RunicDocs › Debug
// drawing › Physics debug view, Path B). Jolt draws through a DebugRenderer; this one writes what
// it is given as plain lines and triangles, either into a frame's DebugGeometry relative to the
// frame's origin (64-bit positions brought down to 32-bit floats one by one, here) or into a
// DebugMesh in a shape's own space. Only builds with Jolt's debug renderer (JPH_DEBUG_RENDERER)
// have one. Private to Runic::Physics.

namespace Runic::Physics::Detail {

class DebugCapture;

// The world keeps its capture behind this, so files that do not draw need no Jolt renderer header.
struct DebugCaptureDeleter {
    void operator()(DebugCapture *capture) const noexcept;
};
using DebugCapturePtr = std::unique_ptr<DebugCapture, DebugCaptureDeleter>;

} // namespace Runic::Physics::Detail
