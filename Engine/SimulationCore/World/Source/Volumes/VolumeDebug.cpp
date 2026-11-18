// The volumes in the debug view (SpatialDebug.hpp's DrawVolumes).

#include <cmath>
#include <vector>

#include "RenderData/DebugDraw.hpp"
#include "SpatialDebug.hpp"
#include "Volumes/VolumeShapes.hpp"

namespace Runic::World {

namespace {

using RenderData::DebugPrimitive;

constexpr std::uint32_t kVerticesPerLine = 2;
constexpr int kAxisX = 0;
constexpr int kAxisY = 1;
constexpr int kAxisZ = 2;

std::uint32_t LinesIn(const RenderData::DebugGeometry &out, RenderData::DebugBatch batch) {
    return out.VertexCount(batch, DebugPrimitive::Lines) / kVerticesPerLine;
}

RenderData::PackedColor ColorOf(VolumeBackend backend) {
    switch (backend) {
    case VolumeBackend::SensorBody:
        return kSensorVolumeColor;
    case VolumeBackend::StaticBody:
        return kSolidVolumeColor;
    case VolumeBackend::SpatialIndex:
        break;
    }
    return kRuleAreaColor;
}

bool Reachable(const SpatialBounds &bounds, Core::WorldPosition origin) {
    const Core::WorldDelta offset = bounds.center - origin;
    const auto reach = [](double along, float half) {
        return std::abs(along) + static_cast<double>(half) < Core::kMaxLocalOffsetM;
    };
    return reach(offset.x, bounds.halfExtents.x) && reach(offset.y, bounds.halfExtents.y) &&
           reach(offset.z, bounds.halfExtents.z);
}

// One part, placed by its volume, camera-relative.
void DrawPart(RenderData::DebugDraw &draw, const VolumeInfo &volume, const VolumePart &part,
              Core::WorldPosition origin, RenderData::PackedColor color) {
    const Detail::DQuat volumeTurn = Detail::ToDouble(volume.rotation);
    const Detail::DVec3 placed = volumeTurn * Detail::ToDouble(part.offset);
    const Core::Vec3 center =
        Core::ToLocal(volume.origin + Core::WorldDelta{placed.x, placed.y, placed.z} - origin);
    const glm::dmat3 axes = glm::mat3_cast(volumeTurn * Detail::ToDouble(part.rotation));
    const Core::Vec3 axisX(axes[kAxisX]);
    const Core::Vec3 axisY(axes[kAxisY]);
    const Core::Vec3 axisZ(axes[kAxisZ]);
    switch (part.shape) {
    case VolumePartShape::Sphere:
        draw.DrawSphere(center, part.radius, color);
        return;
    case VolumePartShape::Cylinder:
        draw.DrawCylinder(center - axisY * part.halfHeight, center + axisY * part.halfHeight,
                          part.radius, color);
        return;
    case VolumePartShape::Box:
        break;
    }
    draw.DrawOBB(center, axisX, axisY, axisZ, part.halfExtents, color);
}

} // namespace

SpatialDebugStats DrawVolumes(const VolumeSet &volumes, Core::WorldPosition origin,
                              RenderData::DebugGeometry &out,
                              const SpatialDebugSettings &settings) {
    SpatialDebugStats stats;
    const std::uint32_t linesBefore = LinesIn(out, settings.batch);
    RenderData::DebugDraw draw(out, settings.batch);
    const SpatialQuery near = SpatialQuery::Sphere(origin, static_cast<float>(settings.radiusM));
    std::vector<VolumeId> ids;
    volumes.Volumes(ids);
    for (const VolumeId id : ids) {
        const VolumeInfo info = volumes.Describe(id);
        if (!near.Overlaps(info.bounds)) {
            continue;
        }
        if (!Reachable(info.bounds, origin)) {
            ++stats.skipped;
            continue;
        }
        const RenderData::PackedColor color = ColorOf(info.backend);
        for (const VolumePart &part : volumes.PartsOf(id)) {
            DrawPart(draw, info, part, origin, color);
            ++stats.parts;
        }
        if (info.indivisible) {
            const Core::Vec3 center = Core::ToLocal(info.bounds.center - origin);
            draw.DrawAABB(center - info.bounds.halfExtents, center + info.bounds.halfExtents,
                          kIndivisibleBoundsColor);
        }
        ++stats.volumes;
    }
    stats.lines = LinesIn(out, settings.batch) - linesBefore;
    return stats;
}

} // namespace Runic::World
