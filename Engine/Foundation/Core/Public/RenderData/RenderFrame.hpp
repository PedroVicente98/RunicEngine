#pragma once

#include <cstdint>

#include "RenderData/Camera.hpp"
#include "RenderData/DebugGeometry.hpp"
#include "RenderData/ShapeInstances.hpp"
#include "WorldSpace.hpp"

// One frame of render data: what the client extracts and only GodotBridge reads (RunicDocs › Render
// data, extraction and world UI › RenderData in Core). Camera-relative: the frame carries one
// 64-bit origin and every position inside it is a 32-bit float relative to that origin. Later
// packages add their kinds next to debug and shapes (CORE-4 to CORE-7).

namespace Runic::RenderData {

// How much one frame holds of each kind.
struct RenderFrameCapacity {
    DebugGeometryCapacity debug;
    std::uint32_t shapeInstances = kDefaultShapeInstanceCapacity;
};

struct RenderFrame {
    explicit RenderFrame(const RenderFrameCapacity &capacity = {})
        : debug(capacity.debug), shapes(capacity.shapeInstances) {}
    explicit RenderFrame(const DebugGeometryCapacity &capacity)
        : RenderFrame(RenderFrameCapacity{capacity, kDefaultShapeInstanceCapacity}) {}

    std::uint64_t number = 0;   // stamped by RenderFrameExchange::Publish: 1 for the first frame
    Core::WorldPosition origin; // everything below is relative to it
    Camera camera;
    DebugGeometry debug;      // lines, triangles and text, redrawn every frame
    ShapeInstanceList shapes; // shapes kept between frames (slice 5)
};

} // namespace Runic::RenderData
