#pragma once

#include <cstdint>
#include <span>

#include "AuthoredVolumes.hpp"
#include "Entity.hpp"
#include "RenderData/Color.hpp"
#include "RenderData/DebugGeometry.hpp"
#include "SpatialIndex.hpp"
#include "WorldSpace.hpp"

// The spatial index and the volumes in the debug view (WORLD-2; RunicDocs › Debug drawing › What
// to visualize: "Cells and interest: AABBs, grid cells, colored boundaries"). Everything near an
// origin comes out as plain camera-relative lines in a Core DebugGeometry, each 64-bit position
// brought down relative to the origin one at a time, so a cell 6,000 km out is drawn to the
// millimetre. GodotBridge draws the geometry; World never sees Godot.

namespace Runic::World {

// What the view draws by default: the cells around a fight and a little beyond.
inline constexpr double kDefaultSpatialDebugRadiusM = 200.0;
// A point member is drawn as a small box this many metres across.
inline constexpr float kPointMarkerHalfM = 0.25f;

// The colors (RunicDocs › Debug drawing: inactive geometry dim gray, server state orange,
// queries magenta, physics blue, sensors cyan).
inline constexpr RenderData::PackedColor kSpatialCellColor = RenderData::kDebugInactive;
inline constexpr RenderData::PackedColor kSpatialMemberColor = RenderData::kDebugAuthoritative;
inline constexpr RenderData::PackedColor kSpatialQueryColor = RenderData::kDebugQuery;
inline constexpr RenderData::PackedColor kSpatialFoundColor = RenderData::kDebugQuery;
inline constexpr RenderData::PackedColor kRuleAreaColor = RenderData::kDebugAuthoritative;
inline constexpr RenderData::PackedColor kSensorVolumeColor = RenderData::kDebugBeneficial;
inline constexpr RenderData::PackedColor kSolidVolumeColor = RenderData::kDebugPhysics;
inline constexpr RenderData::PackedColor kIndivisibleBoundsColor = RenderData::kDebugText;

struct SpatialDebugSettings {
    // Cells, members and volumes whose boxes come within this distance of the origin.
    double radiusM = kDefaultSpatialDebugRadiusM;
    bool cells = true;
    bool members = true;
    RenderData::DebugBatch batch = RenderData::DebugBatch::WorldDepth;
};

struct SpatialDebugStats {
    std::uint32_t cells = 0;
    std::uint32_t members = 0;
    std::uint32_t volumes = 0;
    std::uint32_t parts = 0;
    std::uint32_t lines = 0;
    // Shapes too large to bring down to 32 bits near the origin (more than Core's 10 km): not
    // drawn, counted.
    std::uint32_t skipped = 0;
};

// The occupied cells near the origin as boxes (dim gray), and their members' boxes (orange; a
// point as a small box), in cell order.
SpatialDebugStats DrawSpatialIndex(const SpatialIndex &index, Core::WorldPosition origin,
                                   RenderData::DebugGeometry &out,
                                   const SpatialDebugSettings &settings = {});

// A query's volume (magenta: a sphere, a box or an ellipsoid) and the boxes of the members it
// found, in the overlay batch by default so they show through the cells.
SpatialDebugStats DrawSpatialQuery(const SpatialIndex &index, const SpatialQuery &query,
                                   std::span<const ECS::Entity> found, Core::WorldPosition origin,
                                   RenderData::DebugGeometry &out,
                                   RenderData::DebugBatch batch = RenderData::DebugBatch::Overlay);

// The volumes near the origin, each part in its backend's color (rule areas orange, sensors cyan,
// solids blue); an indivisible volume's bounds in white too.
SpatialDebugStats DrawVolumes(const VolumeSet &volumes, Core::WorldPosition origin,
                              RenderData::DebugGeometry &out,
                              const SpatialDebugSettings &settings = {});

} // namespace Runic::World
