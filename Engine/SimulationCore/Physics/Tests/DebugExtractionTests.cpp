// PHYS-3: Jolt's debug interface inside Physics. Bodies, shapes and characters come out as plain
// camera-relative lines and triangles in a Core DebugGeometry; large shapes become cached debug
// meshes; tools describe bodies and shapes. Physics links no GodotBridge (the module catalog's
// ceiling enforces it at configure time).

#include "PhysicsWorld.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <span>
#include <vector>

using namespace Runic;
using namespace Runic::Physics;
using Core::Vec3;
using Core::WorldPosition;
using RenderData::DebugBatch;
using RenderData::DebugGeometry;
using RenderData::DebugPrimitive;

namespace {

constexpr float kTickSeconds = 1.0f / 30.0f;
// Thousands of kilometres out, where a 32-bit float's step is about half a metre.
const WorldPosition kFarAway{5'000'000.0, 120.0, -7'000'000.0};
// Jolt rounds a box's edges by its convex radius; its debug wireframe stays inside the box.
constexpr float kExactM = 1e-3f;

WorldPosition At(WorldPosition origin, double x, double y, double z) {
    return {origin.x + x, origin.y + y, origin.z + z};
}

struct Bounds {
    Vec3 min{std::numeric_limits<float>::max()};
    Vec3 max{std::numeric_limits<float>::lowest()};
};

Bounds BoundsOf(std::span<const Vec3> points) {
    Bounds bounds;
    for (const Vec3 &p : points) {
        bounds.min = glm::min(bounds.min, p);
        bounds.max = glm::max(bounds.max, p);
    }
    return bounds;
}

bool Skipped(const PhysicsDebugStats &stats) { return !stats.available; }

BodyHandle AddBox(PhysicsWorld &world, std::string_view name, Vec3 half, WorldPosition at,
                  MotionType motion = MotionType::Static, std::uint64_t owner = 0) {
    BodyDesc desc;
    desc.shape = world.AddShape(name, BoxShape(half)).Value();
    desc.motion = motion;
    desc.layer =
        world.Layers().Find(motion == MotionType::Static ? kStaticLayerName : kMovingLayerName);
    desc.pose.position = at;
    desc.owner = owner;
    return world.CreateBody(desc);
}

// A flat square of 2 x cells x cells triangles, as a static mesh.
ShapeId AddGround(PhysicsWorld &world, int cells) {
    std::vector<Vec3> vertices;
    std::vector<std::uint32_t> indices;
    const int side = cells + 1;
    for (int row = 0; row < side; ++row) {
        for (int column = 0; column < side; ++column) {
            vertices.emplace_back(static_cast<float>(column), 0.0f, static_cast<float>(row));
        }
    }
    for (int row = 0; row < cells; ++row) {
        for (int column = 0; column < cells; ++column) {
            const auto at = [side](int r, int c) {
                return static_cast<std::uint32_t>(r * side + c);
            };
            indices.insert(indices.end(),
                           {at(row, column), at(row + 1, column), at(row, column + 1)});
            indices.insert(indices.end(),
                           {at(row, column + 1), at(row + 1, column), at(row + 1, column + 1)});
        }
    }
    return world.AddShape("engine:test_ground", MeshShape(vertices, indices)).Value();
}

} // namespace

TEST(PhysicsDebugView, BodiesComeOutAsPlainLinesWhereTheyAreFarFromTheOrigin) {
    PhysicsWorld world;
    AddBox(world, "engine:test_box", Vec3(1.0f, 0.5f, 2.0f), At(kFarAway, 3.0, 0.5, -4.0));
    DebugGeometry out;
    const PhysicsDebugStats stats = world.ExtractDebugGeometry(kFarAway, {}, out);
    if (Skipped(stats)) {
        GTEST_SKIP() << "this build has no physics debug view";
    }
    EXPECT_EQ(stats.bodies, 1u);
    const auto lines = out.Positions(DebugBatch::WorldDepth, DebugPrimitive::Lines);
    ASSERT_FALSE(lines.empty());
    EXPECT_EQ(out.VertexCount(DebugBatch::WorldDepth, DebugPrimitive::Triangles), 0u); // no faces
    // The wireframe spans the box, relative to the origin, to the millimetre 8,600 km out.
    const Bounds bounds = BoundsOf(lines);
    EXPECT_NEAR(bounds.min.x, 2.0f, kExactM);
    EXPECT_NEAR(bounds.max.x, 4.0f, kExactM);
    EXPECT_NEAR(bounds.min.y, 0.0f, kExactM);
    EXPECT_NEAR(bounds.max.y, 1.0f, kExactM);
    EXPECT_NEAR(bounds.min.z, -6.0f, kExactM);
    EXPECT_NEAR(bounds.max.z, -2.0f, kExactM);
    const auto colors = out.Colors(DebugBatch::WorldDepth, DebugPrimitive::Lines);
    EXPECT_TRUE(std::ranges::all_of(colors, [](auto c) { return c == kStaticBodyColor; }));
}

TEST(PhysicsDebugView, OnlyWhatIsNearTheOriginIsDrawn) {
    PhysicsWorld world;
    AddBox(world, "engine:test_near", Vec3(0.5f), At(kFarAway, 10.0, 0.0, 0.0));
    AddBox(world, "engine:test_far", Vec3(0.5f), At(kFarAway, 500.0, 0.0, 0.0));
    DebugGeometry out;
    PhysicsDebugSettings settings;
    settings.radiusM = 50.0;
    const PhysicsDebugStats stats = world.ExtractDebugGeometry(kFarAway, settings, out);
    if (Skipped(stats)) {
        GTEST_SKIP() << "this build has no physics debug view";
    }
    EXPECT_EQ(stats.bodies, 1u);
    settings.bodies = false;
    out.Clear();
    EXPECT_EQ(world.ExtractDebugGeometry(kFarAway, settings, out).bodies, 0u);
    EXPECT_TRUE(out.IsEmpty());
}

TEST(PhysicsDebugView, ColorsFollowMotionSleepAndSensorsAndFacesAreTranslucent) {
    PhysicsWorld world;
    AddBox(world, "engine:test_floor", Vec3(20.0f, 0.5f, 20.0f), At(kFarAway, 0.0, -0.5, 0.0));
    const BodyHandle crate = AddBox(world, "engine:test_crate", Vec3(0.5f),
                                    At(kFarAway, 0.0, 2.0, 0.0), MotionType::Dynamic);
    PhysicsDebugSettings settings;
    settings.faces = true;
    DebugGeometry out;
    if (Skipped(world.ExtractDebugGeometry(kFarAway, settings, out))) {
        GTEST_SKIP() << "this build has no physics debug view";
    }
    const auto colors = out.Colors(DebugBatch::WorldDepth, DebugPrimitive::Lines);
    EXPECT_TRUE(std::ranges::any_of(colors, [](auto c) { return c == kMovingBodyColor; }));
    const auto faces = out.Colors(DebugBatch::WorldDepth, DebugPrimitive::Triangles);
    ASSERT_FALSE(faces.empty());
    EXPECT_TRUE(std::ranges::all_of(
        faces, [](auto c) { return RenderData::AlphaOf(c) == kDebugFaceAlpha; }));

    for (int tick = 0; tick < 150 && world.IsActive(crate); ++tick) {
        world.Step(kTickSeconds);
    }
    ASSERT_FALSE(world.IsActive(crate)); // asleep on the floor
    out.Clear();
    settings.faces = false;
    world.ExtractDebugGeometry(kFarAway, settings, out);
    const auto asleep = out.Colors(DebugBatch::WorldDepth, DebugPrimitive::Lines);
    EXPECT_TRUE(std::ranges::any_of(asleep, [](auto c) { return c == kSleepingBodyColor; }));
    EXPECT_FALSE(std::ranges::any_of(asleep, [](auto c) { return c == kMovingBodyColor; }));
}

TEST(PhysicsDebugView, CharactersAreDrawnFromTheirFeet) {
    PhysicsWorld world;
    CharacterDesc desc;
    desc.layer = world.Layers().Find(kCharacterLayerName);
    desc.pose.position = At(kFarAway, 1.0, 0.0, 1.0);
    ASSERT_TRUE(world.CreateCharacter(desc).HasValue());
    DebugGeometry out;
    const PhysicsDebugStats stats = world.ExtractDebugGeometry(kFarAway, {}, out);
    if (Skipped(stats)) {
        GTEST_SKIP() << "this build has no physics debug view";
    }
    EXPECT_EQ(stats.characters, 1u);
    const Bounds bounds = BoundsOf(out.Positions(DebugBatch::WorldDepth, DebugPrimitive::Lines));
    EXPECT_NEAR(bounds.min.y, 0.0f, 0.03f); // its feet (Jolt's padding lifts it 2 cm)
    EXPECT_NEAR(bounds.max.y, kDefaultCharacterHeightM, 0.03f);
    EXPECT_NEAR((bounds.min.x + bounds.max.x) / 2.0f, 1.0f, kExactM);
    const auto colors = out.Colors(DebugBatch::WorldDepth, DebugPrimitive::Lines);
    EXPECT_TRUE(std::ranges::all_of(colors, [](auto c) { return c == kCharacterColor; }));
}

TEST(PhysicsDebugView, LargeShapesBecomeCachedMeshesDrawnByInstances) {
    PhysicsWorld world;
    const ShapeId ground = AddGround(world, 16); // 512 triangles
    BodyDesc desc;
    desc.shape = ground;
    desc.layer = world.Layers().Find(kStaticLayerName);
    desc.pose.position = At(kFarAway, -8.0, 0.0, -8.0);
    world.CreateBody(desc);
    AddBox(world, "engine:test_box", Vec3(0.5f), At(kFarAway, 0.0, 0.5, 0.0));

    RenderData::DebugMeshLibrary library;
    DebugGeometry out;
    PhysicsDebugStats stats = world.ExtractDebugGeometry(kFarAway, {}, out, &library);
    if (Skipped(stats)) {
        GTEST_SKIP() << "this build has no physics debug view";
    }
    EXPECT_EQ(stats.bodies, 1u); // the box, as lines
    EXPECT_EQ(stats.meshInstances, 1u);
    EXPECT_EQ(stats.meshesAdded, 1u);
    const Core::DebugMeshId id(kDefaultPhysicsMeshIdBase + ground.Value());
    ASSERT_TRUE(library.Contains(id));
    ASSERT_EQ(out.MeshInstances().size(), 1u);
    const RenderData::DebugMeshInstance &instance = out.MeshInstances()[0];
    EXPECT_EQ(instance.mesh, id);
    EXPECT_NEAR(instance.position.x, -8.0f, kExactM);
    EXPECT_NEAR(instance.position.z, -8.0f, kExactM);
    std::vector<RenderData::DebugMeshLibrary::Entry> entries;
    library.Snapshot(entries);
    ASSERT_EQ(entries.size(), 1u);
    EXPECT_EQ(entries[0].mesh->trianglePositions.size(), 512u * 3u);  // its faces, once
    EXPECT_EQ(entries[0].mesh->linePositions.size(), 512u * 3u * 2u); // and its wireframe

    // The next frame only lists the instance: the library does not change.
    const std::uint64_t revision = library.Revision();
    out.Clear();
    stats = world.ExtractDebugGeometry(kFarAway, {}, out, &library);
    EXPECT_EQ(stats.meshesAdded, 0u);
    EXPECT_EQ(library.Revision(), revision);
    // Without a library the mesh is drawn as lines like any shape.
    out.Clear();
    stats = world.ExtractDebugGeometry(kFarAway, {}, out);
    EXPECT_EQ(stats.bodies, 2u);
    EXPECT_GE(out.VertexCount(DebugBatch::WorldDepth, DebugPrimitive::Lines), 512u * 6u);
}

TEST(PhysicsDebugView, BoundsAndVelocitiesWhenAsked) {
    PhysicsWorld world;
    const BodyHandle crate = AddBox(world, "engine:test_crate", Vec3(0.5f),
                                    At(kFarAway, 0.0, 5.0, 0.0), MotionType::Dynamic);
    world.SetLinearVelocity(crate, Vec3(3.0f, 0.0f, 0.0f));
    DebugGeometry plain;
    if (Skipped(world.ExtractDebugGeometry(kFarAway, {}, plain))) {
        GTEST_SKIP() << "this build has no physics debug view";
    }
    PhysicsDebugSettings settings;
    settings.boundingBoxes = true;
    settings.velocities = true;
    settings.batch = DebugBatch::Overlay;
    DebugGeometry more;
    world.ExtractDebugGeometry(kFarAway, settings, more);
    EXPECT_EQ(more.VertexCount(DebugBatch::WorldDepth, DebugPrimitive::Lines), 0u);
    EXPECT_GT(more.VertexCount(DebugBatch::Overlay, DebugPrimitive::Lines),
              plain.VertexCount(DebugBatch::WorldDepth, DebugPrimitive::Lines) + 24u);
    const auto colors = more.Colors(DebugBatch::Overlay, DebugPrimitive::Lines);
    EXPECT_TRUE(std::ranges::any_of(colors, [](auto c) { return c == kVelocityColor; }));
    // The arrow reaches a second ahead: 3 m along +X.
    const Bounds bounds = BoundsOf(more.Positions(DebugBatch::Overlay, DebugPrimitive::Lines));
    EXPECT_NEAR(bounds.max.x, 3.0f, 0.05f);
}

TEST(PhysicsInspection, DescribesBodiesAndShapesForTools) {
    PhysicsWorld world;
    const BodyHandle floor = AddBox(world, "engine:test_floor", Vec3(5.0f, 0.5f, 5.0f),
                                    At(kFarAway, 0.0, -0.5, 0.0), MotionType::Static, 11);
    const BodyHandle crate = AddBox(world, "engine:test_crate", Vec3(0.5f),
                                    At(kFarAway, 1.0, 3.0, 0.0), MotionType::Dynamic, 12);
    AddBox(world, "engine:test_distant", Vec3(0.5f), At(kFarAway, 900.0, 0.0, 0.0));
    const ShapeId ground = AddGround(world, 2);

    std::vector<BodyHandle> near;
    world.FindBodies(kFarAway, 20.0, near);
    ASSERT_EQ(near.size(), 2u);
    EXPECT_EQ(near[0], floor);
    EXPECT_EQ(near[1], crate);

    const BodyInfo info = world.DescribeBody(crate);
    EXPECT_EQ(info.body, crate);
    EXPECT_EQ(info.motion, MotionType::Dynamic);
    EXPECT_EQ(info.layer, world.Layers().Find(kMovingLayerName));
    EXPECT_EQ(info.owner, 12u);
    EXPECT_TRUE(info.active);
    EXPECT_FALSE(info.sensor);
    EXPECT_NEAR(info.pose.position.x - kFarAway.x, 1.0, 1e-9);
    const ShapeInfo shape = world.DescribeShape(info.shape);
    EXPECT_EQ(shape.name, "engine:test_crate");
    EXPECT_EQ(shape.kind, ShapeKind::Box);
    EXPECT_NEAR(shape.halfExtents.x, 0.5f, kExactM);
    const ShapeInfo mesh = world.DescribeShape(ground);
    EXPECT_EQ(mesh.kind, ShapeKind::Mesh);
    EXPECT_EQ(mesh.triangles, 8u);
}
