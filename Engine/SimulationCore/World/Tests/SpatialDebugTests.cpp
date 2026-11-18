// WORLD-2: the spatial index, its queries and the volumes in the debug view, as plain lines
// relative to an origin far out.

#include "SpatialDebug.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

#include "RenderData/DebugDraw.hpp"

using namespace Runic;
using namespace Runic::World;
using Core::Vec3;
using Core::WorldPosition;
using RenderData::DebugBatch;
using RenderData::DebugGeometry;
using RenderData::DebugPrimitive;

namespace {

const WorldPosition kFarAway{6'000'000.0, 0.0, -5'000'000.0};
constexpr float kMillimetre = 0.001f;

WorldPosition At(double x, double y, double z) {
    return {kFarAway.x + x, kFarAway.y + y, kFarAway.z + z};
}

ECS::Entity Member(std::uint64_t n) { return ECS::Entity::FromRaw(n); }

std::uint32_t Lines(const DebugGeometry &geometry, DebugBatch batch) {
    return geometry.VertexCount(batch, DebugPrimitive::Lines) / 2;
}

struct Span {
    Vec3 min{1e9f};
    Vec3 max{-1e9f};
};

// The box around the vertices of one color.
Span SpanOf(const DebugGeometry &geometry, DebugBatch batch, RenderData::PackedColor color) {
    Span span;
    const auto positions = geometry.Positions(batch, DebugPrimitive::Lines);
    const auto colors = geometry.Colors(batch, DebugPrimitive::Lines);
    for (std::size_t i = 0; i < positions.size(); ++i) {
        if (colors[i] == color) {
            span.min = glm::min(span.min, positions[i]);
            span.max = glm::max(span.max, positions[i]);
        }
    }
    return span;
}

} // namespace

TEST(SpatialDebug, CellsAndMembersComeOutAsLinesRelativeToTheOrigin) {
    SpatialIndex index; // 64 m cells
    index.Insert(Member(1), {At(10.0, 10.0, 10.0)});
    index.Insert(Member(2), {At(20.0, 15.0, 10.0), Vec3(2.0f, 1.0f, 3.0f)});
    DebugGeometry geometry;
    // The view's origin a little off the cell's corner: every line is relative to it.
    const WorldPosition origin = At(5.0, 2.0, -3.0);
    const SpatialDebugStats stats = DrawSpatialIndex(index, origin, geometry);
    EXPECT_EQ(stats.cells, 1u);
    EXPECT_EQ(stats.members, 2u);
    EXPECT_EQ(stats.lines, 3 * RenderData::kBoxLines);
    EXPECT_EQ(Lines(geometry, DebugBatch::WorldDepth), stats.lines);
    // The cell spans (0, 0, 0) to (64, 64, 64) from the far-away corner, to the millimetre.
    const Span cell = SpanOf(geometry, DebugBatch::WorldDepth, kSpatialCellColor);
    EXPECT_NEAR(cell.min.x, -5.0f, kMillimetre);
    EXPECT_NEAR(cell.min.y, -2.0f, kMillimetre);
    EXPECT_NEAR(cell.min.z, 3.0f, kMillimetre);
    EXPECT_NEAR(cell.max.x, 59.0f, kMillimetre);
    EXPECT_NEAR(cell.max.y, 62.0f, kMillimetre);
    EXPECT_NEAR(cell.max.z, 67.0f, kMillimetre);
    // The members: a point's small box and the box member's own.
    const Span members = SpanOf(geometry, DebugBatch::WorldDepth, kSpatialMemberColor);
    EXPECT_NEAR(members.min.x, 5.0f - kPointMarkerHalfM, kMillimetre);
    EXPECT_NEAR(members.max.x, 17.0f, kMillimetre);
    EXPECT_NEAR(members.max.z, 16.0f, kMillimetre);
}

TEST(SpatialDebug, OnlyWhatIsNearTheOriginIsDrawn) {
    SpatialIndex index;
    index.Insert(Member(1), {At(10.0, 10.0, 10.0)});
    index.Insert(Member(2), {At(900.0, 10.0, 10.0)});  // 14 cells away
    index.Insert(Member(3), {At(10.0, -600.0, 10.0)}); // deep under the origin
    DebugGeometry geometry;
    SpatialDebugSettings settings;
    settings.radiusM = 100.0;
    const SpatialDebugStats stats = DrawSpatialIndex(index, At(0.0, 0.0, 0.0), geometry, settings);
    EXPECT_EQ(stats.cells, 1u);
    EXPECT_EQ(stats.members, 1u);
    settings.members = false;
    DebugGeometry cellsOnly;
    EXPECT_EQ(DrawSpatialIndex(index, At(0.0, 0.0, 0.0), cellsOnly, settings).lines,
              RenderData::kBoxLines);
}

TEST(SpatialDebug, AQueryDrawsItsVolumeAndWhatItFound) {
    SpatialIndex index;
    index.Insert(Member(1), {At(10.0, 0.0, 0.0)});
    index.Insert(Member(2), {At(0.0, 0.0, 30.0)});
    const SpatialQuery sphere = SpatialQuery::Sphere(At(0.0, 0.0, 0.0), 12.0f);
    std::vector<ECS::Entity> found;
    index.Query(sphere, found);
    ASSERT_EQ(found.size(), 1u);
    DebugGeometry geometry;
    const SpatialDebugStats stats =
        DrawSpatialQuery(index, sphere, found, At(0.0, 0.0, 0.0), geometry);
    EXPECT_EQ(stats.members, 1u);
    EXPECT_EQ(stats.lines,
              RenderData::SphereLines(RenderData::kDefaultRoundSegments) + RenderData::kBoxLines);
    EXPECT_EQ(Lines(geometry, DebugBatch::Overlay), stats.lines);
    EXPECT_EQ(Lines(geometry, DebugBatch::WorldDepth), 0u);
    const Span span = SpanOf(geometry, DebugBatch::Overlay, kSpatialQueryColor);
    EXPECT_NEAR(span.max.x, 12.0f, 0.01f);
    EXPECT_NEAR(span.min.z, -12.0f, 0.01f);

    // An ellipsoid: three ellipses, as wide and as tall as its radii.
    DebugGeometry flat;
    const SpatialQuery ellipsoid =
        SpatialQuery::Ellipsoid(At(0.0, 0.0, 0.0), Vec3(40.0f, 8.0f, 40.0f));
    DrawSpatialQuery(index, ellipsoid, {}, At(0.0, 0.0, 0.0), flat);
    const Span ellipse = SpanOf(flat, DebugBatch::Overlay, kSpatialQueryColor);
    EXPECT_NEAR(ellipse.max.x, 40.0f, 0.01f);
    EXPECT_NEAR(ellipse.max.y, 8.0f, 0.01f);
    EXPECT_NEAR(ellipse.min.y, -8.0f, 0.01f);
    // A box query: one box.
    DebugGeometry box;
    EXPECT_EQ(DrawSpatialQuery(index, SpatialQuery::Box(At(0.0, 0.0, 0.0), Vec3(3.0f)), {},
                               At(0.0, 0.0, 0.0), box)
                  .lines,
              RenderData::kBoxLines);
}

TEST(SpatialDebug, VolumesDrawInTheirBackendsColor) {
    VolumeSet volumes;
    const VolumeKindId cave = volumes.AddKind({"engine:test_cave"}).Value();
    const VolumeKindId deck = volumes.AddKind({"engine:test_deck", false, true}).Value();
    const VolumeKindId wall = volumes.AddKind({"engine:test_wall", true, false}).Value();
    VolumeDesc desc;
    desc.kind = cave;
    desc.origin = At(0.0, -50.0, 0.0);
    desc.parts = {BoxPart(Vec3(20.0f, 3.0f, 3.0f)), SpherePart(8.0f, Vec3(20.0f, 0.0f, 0.0f))};
    desc.indivisible = true;
    volumes.Add(desc).Value();
    desc.kind = deck;
    desc.origin = At(0.0, 2.0, 0.0);
    desc.parts = {BoxPart(Vec3(10.0f, 1.0f, 3.0f))};
    desc.indivisible = false;
    volumes.Add(desc).Value();
    desc.kind = wall;
    desc.origin = At(-30.0, 0.0, 0.0);
    desc.parts = {CylinderPart(2.0f, 5.0f)};
    volumes.Add(desc).Value();
    desc.kind = cave;
    desc.origin = At(5'000.0, 0.0, 0.0); // far from the origin
    volumes.Add(desc).Value();

    DebugGeometry geometry;
    const SpatialDebugStats stats = DrawVolumes(volumes, At(0.0, 0.0, 0.0), geometry);
    EXPECT_EQ(stats.volumes, 3u);
    EXPECT_EQ(stats.parts, 4u);
    const std::uint32_t expected = RenderData::kBoxLines +
                                   RenderData::SphereLines(RenderData::kDefaultRoundSegments) +
                                   RenderData::kBoxLines + // the indivisible cave's bounds
                                   RenderData::kBoxLines + // the deck
                                   RenderData::CylinderLines(RenderData::kDefaultRoundSegments);
    EXPECT_EQ(stats.lines, expected);
    EXPECT_EQ(Lines(geometry, DebugBatch::WorldDepth), expected);
    const Span rule = SpanOf(geometry, DebugBatch::WorldDepth, kRuleAreaColor);
    EXPECT_NEAR(rule.min.x, -20.0f, 0.01f);
    EXPECT_NEAR(rule.max.x, 28.0f, 0.01f); // the chamber's far side
    EXPECT_NEAR(rule.min.y, -58.0f, 0.01f);
    const Span sensor = SpanOf(geometry, DebugBatch::WorldDepth, kSensorVolumeColor);
    EXPECT_NEAR(sensor.max.y, 3.0f, kMillimetre);
    const Span solid = SpanOf(geometry, DebugBatch::WorldDepth, kSolidVolumeColor);
    EXPECT_NEAR(solid.min.x, -32.0f, 0.01f);
    const Span bounds = SpanOf(geometry, DebugBatch::WorldDepth, kIndivisibleBoundsColor);
    EXPECT_NEAR(bounds.max.x, 28.0f, 0.01f);
    EXPECT_NEAR(bounds.max.y, -42.0f, 0.01f);
}
