#pragma once

#include <cstdint>

#include "RenderData/Camera.hpp"
#include "RenderData/DebugGeometry.hpp"
#include "WorldSpace.hpp"

// One frame of render data: what the client extracts and only GodotBridge reads (RunicDocs › Render
// data, extraction and world UI › RenderData in Core). Camera-relative: the frame carries one
// 64-bit origin and every position inside it is a 32-bit float relative to that origin. Later
// packages add their kinds next to debug (CORE-4 to CORE-7).

namespace Runic::RenderData {

struct RenderFrame {
    explicit RenderFrame(const DebugGeometryCapacity &capacity = {}) : debug(capacity) {}

    std::uint64_t number = 0;   // stamped by RenderFrameExchange::Publish: 1 for the first frame
    Core::WorldPosition origin; // everything below is relative to it
    Camera camera;
    DebugGeometry debug; // lines, triangles and text
};

} // namespace Runic::RenderData
