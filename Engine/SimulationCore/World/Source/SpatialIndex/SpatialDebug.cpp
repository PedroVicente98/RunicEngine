// The spatial index and its queries in the debug view (SpatialDebug.hpp).

#include "SpatialDebug.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <vector>

#include "RenderData/DebugDraw.hpp"

namespace Runic::World {

namespace {

using RenderData::DebugPrimitive;

constexpr std::uint32_t kVerticesPerLine = 2;

std::uint32_t LinesIn(const RenderData::DebugGeometry &out, RenderData::DebugBatch batch) {
    return out.VertexCount(batch, DebugPrimitive::Lines) / kVerticesPerLine;
}

// Whether a box can come down to 32 bits relative to the origin (Core's ToLocal bound).
bool Reachable(const SpatialBounds &bounds, Core::WorldPosition origin) {
    const Core::WorldDelta offset = bounds.center - origin;
    const auto reach = [](double along, float half) {
        return std::abs(along) + static_cast<double>(half) < Core::kMaxLocalOffsetM;
    };
    return reach(offset.x, bounds.halfExtents.x) && reach(offset.y, bounds.halfExtents.y) &&
           reach(offset.z, bounds.halfExtents.z);
}

void DrawBox(RenderData::DebugDraw &draw, const SpatialBounds &bounds, Core::WorldPosition origin,
             RenderData::PackedColor color) {
    const Core::Vec3 center = Core::ToLocal(bounds.center - origin);
    draw.DrawAABB(center - bounds.halfExtents, center + bounds.halfExtents, color);
}

// A point is drawn as a small box.
SpatialBounds Shown(const SpatialBounds &bounds) {
    const Core::Vec3 marker(kPointMarkerHalfM);
    return {bounds.center, glm::max(bounds.halfExtents, marker)};
}

// The three ellipses of an axis-aligned ellipsoid, in the planes through its center.
void DrawEllipsoid(RenderData::DebugDraw &draw, Core::Vec3 center, Core::Vec3 radii,
                   RenderData::PackedColor color) {
    constexpr std::uint32_t kSegments = RenderData::kDefaultRoundSegments;
    constexpr int kPlanes = 3;
    for (int plane = 0; plane < kPlanes; ++plane) {
        // The plane's two axes: (x, y), (y, z) and (z, x).
        const int first = plane;
        const int second = (plane + 1) % kPlanes;
        Core::Vec3 previous = center;
        previous[first] += radii[first];
        for (std::uint32_t i = 1; i <= kSegments; ++i) {
            const double angle = 2.0 * std::numbers::pi * i / kSegments;
            Core::Vec3 next = center;
            next[first] += radii[first] * static_cast<float>(std::cos(angle));
            next[second] += radii[second] * static_cast<float>(std::sin(angle));
            draw.DrawLine(previous, next, color);
            previous = next;
        }
    }
}

} // namespace

SpatialDebugStats DrawSpatialIndex(const SpatialIndex &index, Core::WorldPosition origin,
                                   RenderData::DebugGeometry &out,
                                   const SpatialDebugSettings &settings) {
    SpatialDebugStats stats;
    const std::uint32_t linesBefore = LinesIn(out, settings.batch);
    RenderData::DebugDraw draw(out, settings.batch);
    const CoordinateSpace &space = index.Space();
    const SpatialQuery near = SpatialQuery::Sphere(origin, static_cast<float>(settings.radiusM));
    const CellRange range = CellsTouched(space, near.Bounds());
    const auto cellSize = static_cast<float>(space.cellSizeM);
    const Core::Vec3 cellHalf(cellSize / 2.0f);

    std::vector<SpatialCellInfo> cells;
    index.Cells(range, cells);
    std::vector<ECS::Entity> members;
    for (const SpatialCellInfo &cell : cells) {
        const SpatialBounds box{space.CellCenter(cell.cell), cellHalf};
        if (!near.Overlaps(box)) {
            continue;
        }
        if (!Reachable(box, origin)) {
            ++stats.skipped;
            continue;
        }
        if (settings.cells) {
            DrawBox(draw, box, origin, kSpatialCellColor);
            ++stats.cells;
        }
        if (!settings.members) {
            continue;
        }
        members.clear();
        index.MembersOf(cell.cell, members);
        for (const ECS::Entity member : members) {
            // A member in several cells is drawn from the first of them near the origin.
            const CellRange in = index.CellsOf(member);
            const CellCoord first{std::max(in.min.x, range.min.x), std::max(in.min.y, range.min.y),
                                  std::max(in.min.z, range.min.z)};
            const SpatialBounds shown = Shown(index.BoundsOf(member));
            if (!(first == cell.cell) || !near.Overlaps(shown)) {
                continue;
            }
            if (!Reachable(shown, origin)) {
                ++stats.skipped;
                continue;
            }
            DrawBox(draw, shown, origin, kSpatialMemberColor);
            ++stats.members;
        }
    }
    stats.lines = LinesIn(out, settings.batch) - linesBefore;
    return stats;
}

SpatialDebugStats DrawSpatialQuery(const SpatialIndex &index, const SpatialQuery &query,
                                   std::span<const ECS::Entity> found, Core::WorldPosition origin,
                                   RenderData::DebugGeometry &out, RenderData::DebugBatch batch) {
    SpatialDebugStats stats;
    const std::uint32_t linesBefore = LinesIn(out, batch);
    RenderData::DebugDraw draw(out, batch);
    if (Reachable(query.Bounds(), origin)) {
        const Core::Vec3 center = Core::ToLocal(query.center - origin);
        switch (query.shape) {
        case QueryShape::Sphere:
            draw.DrawSphere(center, query.extents.x, kSpatialQueryColor);
            break;
        case QueryShape::Box:
            draw.DrawAABB(center - query.extents, center + query.extents, kSpatialQueryColor);
            break;
        case QueryShape::Ellipsoid:
            DrawEllipsoid(draw, center, query.extents, kSpatialQueryColor);
            break;
        }
    } else {
        ++stats.skipped;
    }
    for (const ECS::Entity member : found) {
        if (!index.Contains(member)) {
            continue; // gone since the query
        }
        const SpatialBounds shown = Shown(index.BoundsOf(member));
        if (!Reachable(shown, origin)) {
            ++stats.skipped;
            continue;
        }
        DrawBox(draw, shown, origin, kSpatialFoundColor);
        ++stats.members;
    }
    stats.lines = LinesIn(out, batch) - linesBefore;
    return stats;
}

} // namespace Runic::World
