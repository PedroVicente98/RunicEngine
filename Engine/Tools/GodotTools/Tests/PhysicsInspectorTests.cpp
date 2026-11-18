// GDT-3: the physics inspector's model over a local simulation with physics, without Godot. Bodies
// are listed near a point, inspected with their properties, picked by a ray and selected; the "see
// what the server sees" geometry is the physics debug view, over the visuals or depth-tested, with
// the selected body's bounds on top.

#include "PhysicsInspector.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include <glm/gtc/quaternion.hpp>

#include "EntityWorld.hpp"
#include "PhysicsBridge.hpp"
#include "RenderData/DebugDraw.hpp"
#include "SimulationRuntime.hpp"
#include "WorldComponents.hpp"
#include "WorldInspector.hpp"

using namespace Runic;
using namespace Runic::GodotTools;
using Core::Vec3;
using Core::WorldPosition;
using RenderData::DebugBatch;
using RenderData::DebugPrimitive;

namespace {

// Thousands of kilometres out: what the dock shows is world positions, never float offsets.
const WorldPosition kFarAway{5'000'000.0, 120.0, -7'000'000.0};

WorldPosition At(double x, double y, double z) {
    return {kFarAway.x + x, kFarAway.y + y, kFarAway.z + z};
}

void RunRealtimeTick(Simulation::SimulationRuntime &runtime) {
    const std::uint64_t before = runtime.TicksRun(Simulation::TickDomain::Hz30);
    while (runtime.TicksRun(Simulation::TickDomain::Hz30) == before) {
        runtime.RunMasterTick();
    }
}

// A small scene: a 20 m static floor whose top is at kFarAway's height, a 2 m kinematic box
// standing on it at (3, 1, -2), a static 1 m sphere at (-4, 1, 0); each body an entity's.
struct Scene {
    Simulation::SimulationRuntime runtime;
    Simulation::PhysicsBridge &bridge = runtime.EnablePhysics();
    ECS::Entity floor;
    ECS::Entity box;
    ECS::Entity sphere;

    Scene() {
        floor = Add("engine:test_floor", Physics::BoxShape(Vec3(10.0f, 0.5f, 10.0f)),
                    Physics::MotionType::Static, At(0.0, -0.5, 0.0));
        box = Add("engine:test_box", Physics::BoxShape(Vec3(1.0f)), Physics::MotionType::Kinematic,
                  At(3.0, 1.0, -2.0));
        sphere = Add("engine:test_sphere", Physics::SphereShape(0.5f), Physics::MotionType::Static,
                     At(-4.0, 1.0, 0.0));
        RunRealtimeTick(runtime);
    }

    ECS::Entity Add(std::string_view shape, const Physics::ShapeDesc &desc,
                    Physics::MotionType motion, WorldPosition where,
                    Core::Quat rotation = Core::Quat(1.0f, 0.0f, 0.0f, 0.0f)) {
        Physics::PhysicsWorld &world = bridge.World();
        Physics::BodyDesc body;
        body.shape = world.AddShape(shape, desc).Value();
        body.motion = motion;
        body.layer =
            world.Layers().Find(motion == Physics::MotionType::Static ? Physics::kStaticLayerName
                                                                      : Physics::kMovingLayerName);
        const ECS::Entity entity = runtime.Entities().Create();
        runtime.Entities().Set(entity, World::WorldTransform{where, rotation});
        bridge.AddBody(entity, body);
        return entity;
    }

    Physics::BodyHandle Body(ECS::Entity entity) {
        return runtime.Entities().Get<Simulation::PhysicsBody>(entity).handle;
    }
};

std::string Property(const InspectedBody &body, std::string_view name) {
    for (const InspectedProperty &property : body.properties) {
        if (property.name == name) {
            return property.value;
        }
    }
    return "(missing)";
}

struct Bounds {
    Vec3 min{1e9f};
    Vec3 max{-1e9f};
    int vertices = 0;
};

// The bounds of the vertices of one color in one batch's lines.
Bounds LinesOfColor(const RenderData::DebugGeometry &geometry, DebugBatch batch,
                    RenderData::PackedColor color) {
    Bounds bounds;
    const auto positions = geometry.Positions(batch, DebugPrimitive::Lines);
    const auto colors = geometry.Colors(batch, DebugPrimitive::Lines);
    for (std::size_t i = 0; i < positions.size(); ++i) {
        if (colors[i] == color) {
            bounds.min = glm::min(bounds.min, positions[i]);
            bounds.max = glm::max(bounds.max, positions[i]);
            ++bounds.vertices;
        }
    }
    return bounds;
}

} // namespace

TEST(PhysicsInspector, ASimulationWithoutPhysicsShowsNothing) {
    Simulation::SimulationRuntime runtime;
    PhysicsInspector inspector(runtime);
    EXPECT_FALSE(inspector.Summary().enabled);
    EXPECT_TRUE(inspector.Bodies(kFarAway).empty());
    EXPECT_FALSE(inspector.Pick(kFarAway, Vec3(0.0f, -1.0f, 0.0f)).IsValid());
    RenderData::DebugGeometry geometry;
    const ServerViewStats stats = inspector.DrawServerView(kFarAway, {}, geometry);
    EXPECT_FALSE(stats.physics.available);
    EXPECT_TRUE(geometry.IsEmpty());
}

TEST(PhysicsInspector, SummarizesThePhysicsWorld) {
    Scene scene;
    const PhysicsSummary summary = PhysicsInspector(scene.runtime).Summary();
    EXPECT_TRUE(summary.enabled);
    EXPECT_EQ(summary.bodies, 3u);
    EXPECT_EQ(summary.shapes, 3u);
    EXPECT_EQ(summary.entityBodies, 3u);
    EXPECT_EQ(summary.characters, 0u);
    EXPECT_GE(summary.steps, 1u);
}

TEST(PhysicsInspector, ListsTheBodiesNearAPointInHandleOrder) {
    Scene scene;
    const ECS::Entity faraway = scene.Add("engine:test_far_box", Physics::BoxShape(Vec3(0.5f)),
                                          Physics::MotionType::Static, At(2'000.0, 0.0, 0.0));
    RunRealtimeTick(scene.runtime);
    PhysicsInspector inspector(scene.runtime);

    const auto near = inspector.Bodies(kFarAway);
    ASSERT_EQ(near.size(), 3u);
    EXPECT_TRUE(std::is_sorted(near.begin(), near.end(),
                               [](auto a, auto b) { return a.Value() < b.Value(); }));
    EXPECT_EQ(std::count(near.begin(), near.end(), scene.Body(faraway)), 0);
    EXPECT_EQ(inspector.Bodies(At(2'000.0, 0.0, 0.0), 10.0).size(), 1u);
    EXPECT_EQ(inspector.Bodies(kFarAway, kDefaultInspectRadiusM, 2).size(), 2u);
}

TEST(PhysicsInspector, ShowsABodysPropertiesInWorldSpace) {
    Scene scene;
    PhysicsInspector inspector(scene.runtime);
    const InspectedBody box = inspector.Inspect(scene.Body(scene.box));
    EXPECT_EQ(box.label, "body " + std::to_string(scene.Body(scene.box).Value()));
    EXPECT_EQ(box.shape, "engine:test_box");
    EXPECT_EQ(box.motion, "kinematic");

    std::vector<std::string> names;
    for (const InspectedProperty &property : box.properties) {
        names.push_back(property.name);
    }
    EXPECT_EQ(names, (std::vector<std::string>{"shape", "kind", "half extents", "motion", "layer",
                                               "position", "rotation", "velocity", "friction",
                                               "restitution", "state", "sensor", "owner"}));
    EXPECT_EQ(Property(box, kBodyShapeProperty), "engine:test_box");
    EXPECT_EQ(Property(box, kBodyKindProperty), "box");
    EXPECT_EQ(Property(box, kBodyHalfExtentsProperty), "1.000, 1.000, 1.000 m");
    EXPECT_EQ(Property(box, kBodyLayerProperty), "engine:moving");
    EXPECT_EQ(Property(box, kBodyPositionProperty), "5000003.000, 121.000, -7000002.000");
    EXPECT_EQ(Property(box, kBodyRotationProperty), "w 1.000, x 0.000, y 0.000, z 0.000");
    EXPECT_EQ(Property(box, kBodyVelocityProperty), "0.000, 0.000, 0.000 m/s");
    EXPECT_EQ(Property(box, kBodyFrictionProperty), "0.50");
    EXPECT_EQ(Property(box, kBodyRestitutionProperty), "0.00");
    EXPECT_EQ(Property(box, kBodySensorProperty), "no");
    // The owner is named as the world dock names the entity.
    EXPECT_EQ(Property(box, kBodyOwnerProperty),
              WorldInspector(scene.runtime).Inspect(scene.box).label);

    const InspectedBody floor = inspector.Inspect(scene.Body(scene.floor));
    EXPECT_EQ(Property(floor, kBodyMotionProperty), "static");
    EXPECT_EQ(Property(floor, kBodyStateProperty), "never moves");
    EXPECT_EQ(Property(inspector.Inspect(scene.Body(scene.sphere)), kBodyKindProperty), "sphere");
}

TEST(PhysicsInspector, AGoneBodyHasNoPropertiesAndIsNoLongerSelected) {
    Scene scene;
    PhysicsInspector inspector(scene.runtime);
    const Physics::BodyHandle box = scene.Body(scene.box);
    inspector.Select(box);
    EXPECT_EQ(inspector.Selected(), box);

    scene.runtime.Entities().Destroy(scene.box);
    RunRealtimeTick(scene.runtime);
    EXPECT_TRUE(inspector.Inspect(box).properties.empty());
    EXPECT_FALSE(inspector.Selected().IsValid());
    EXPECT_EQ(inspector.Bodies(kFarAway).size(), 2u);
}

TEST(PhysicsInspector, PicksTheNearestBodyARayMeets) {
    Scene scene;
    PhysicsInspector inspector(scene.runtime);
    const Vec3 down(0.0f, -1.0f, 0.0f);
    // From above: the box, not the floor under it; the sphere; the floor where nothing stands.
    EXPECT_EQ(inspector.Pick(At(3.0, 20.0, -2.0), down), scene.Body(scene.box));
    EXPECT_EQ(inspector.Pick(At(-4.0, 20.0, 0.0), down), scene.Body(scene.sphere));
    EXPECT_EQ(inspector.Pick(At(6.0, 20.0, 6.0), down), scene.Body(scene.floor));
    // Sideways, and a direction that need not be a unit vector.
    EXPECT_EQ(inspector.Pick(At(-20.0, 1.0, -2.0), Vec3(5.0f, 0.0f, 0.0f)), scene.Body(scene.box));
    // Past the sphere's silhouette but inside its bounds' corner: the floor below it.
    EXPECT_EQ(inspector.Pick(At(-4.45, 20.0, 0.45), down), scene.Body(scene.floor));
    // Pointing away, or too short to reach.
    EXPECT_FALSE(inspector.Pick(At(3.0, 20.0, -2.0), Vec3(0.0f, 1.0f, 0.0f)).IsValid());
    EXPECT_FALSE(inspector.Pick(At(3.0, 20.0, -2.0), down, 5.0).IsValid());
}

TEST(PhysicsInspector, PicksATurnedBoxByItsOwnShape) {
    Scene scene;
    // A 2 m box turned 45° about y, far from the others: its corners reach 1.41 m along x.
    const ECS::Entity turned = scene.Add(
        "engine:test_turned_box", Physics::BoxShape(Vec3(1.0f)), Physics::MotionType::Static,
        At(40.0, 1.0, 0.0), glm::angleAxis(glm::radians(45.0f), Vec3(0.0f, 1.0f, 0.0f)));
    RunRealtimeTick(scene.runtime);
    PhysicsInspector inspector(scene.runtime);
    const Vec3 down(0.0f, -1.0f, 0.0f);
    // Over a corner the turned box reaches (1.3 m along x), and over a spot its bounds cover but
    // the box does not (1.2 m along x and z).
    EXPECT_EQ(inspector.Pick(At(41.3, 20.0, 0.0), down), scene.Body(turned));
    EXPECT_NE(inspector.Pick(At(41.2, 20.0, 1.2), down), scene.Body(turned));
}

TEST(PhysicsInspector, AMeshIsNotPickedByARay) {
    Simulation::SimulationRuntime runtime;
    Simulation::PhysicsBridge &bridge = runtime.EnablePhysics();
    Physics::PhysicsWorld &world = bridge.World();
    // A 20 m square of two triangles at kFarAway's height.
    Physics::BodyDesc desc;
    desc.shape = world
                     .AddShape("engine:test_ground", Physics::MeshShape({{-10.0f, 0.0f, -10.0f},
                                                                         {-10.0f, 0.0f, 10.0f},
                                                                         {10.0f, 0.0f, 10.0f},
                                                                         {10.0f, 0.0f, -10.0f}},
                                                                        {0, 1, 2, 0, 2, 3}))
                     .Value();
    desc.layer = world.Layers().Find(Physics::kStaticLayerName);
    const ECS::Entity ground = runtime.Entities().Create();
    runtime.Entities().Set(ground, World::WorldTransform{kFarAway});
    bridge.AddBody(ground, desc);
    RunRealtimeTick(runtime);

    PhysicsInspector inspector(runtime);
    ASSERT_EQ(inspector.Bodies(kFarAway).size(), 1u);
    EXPECT_FALSE(inspector.Pick(At(0.0, 20.0, 0.0), Vec3(0.0f, -1.0f, 0.0f)).IsValid());
    EXPECT_EQ(Property(inspector.Inspect(inspector.Bodies(kFarAway).front()), kBodyKindProperty),
              "mesh, 2 triangles");
}

TEST(PhysicsInspector, TheServerViewDrawsOverTheVisualsOrDepthTested) {
    Scene scene;
    PhysicsInspector inspector(scene.runtime);
    RenderData::DebugGeometry over;
    const ServerViewStats overStats = inspector.DrawServerView(kFarAway, {}, over);
    if (!overStats.physics.available) {
        GTEST_SKIP() << "this build has no physics debug view";
    }
    EXPECT_EQ(overStats.physics.bodies, 3u);
    EXPECT_FALSE(overStats.selectionDrawn);
    EXPECT_GT(over.VertexCount(DebugBatch::Overlay, DebugPrimitive::Lines), 0u);
    EXPECT_EQ(over.VertexCount(DebugBatch::WorldDepth, DebugPrimitive::Lines), 0u);
    // Relative to the origin: the kinematic box's lines span its 2 m around (3, 1, -2).
    const Bounds box = LinesOfColor(over, DebugBatch::Overlay, Physics::kMovingBodyColor);
    ASSERT_GT(box.vertices, 0);
    EXPECT_NEAR(box.min.x, 2.0f, 1e-3f);
    EXPECT_NEAR(box.max.x, 4.0f, 1e-3f);
    EXPECT_NEAR(box.max.y, 2.0f, 1e-3f);
    EXPECT_NEAR(box.min.z, -3.0f, 1e-3f);

    ServerViewSettings depth;
    depth.overVisuals = false;
    RenderData::DebugGeometry tested;
    inspector.DrawServerView(kFarAway, depth, tested);
    EXPECT_EQ(tested.VertexCount(DebugBatch::Overlay, DebugPrimitive::Lines), 0u);
    EXPECT_EQ(tested.VertexCount(DebugBatch::WorldDepth, DebugPrimitive::Lines),
              over.VertexCount(DebugBatch::Overlay, DebugPrimitive::Lines));
}

TEST(PhysicsInspector, TheSelectedBodysBoundsDrawOverEverything) {
    Scene scene;
    PhysicsInspector inspector(scene.runtime);
    inspector.Select(scene.Body(scene.box));
    ServerViewSettings depth;
    depth.overVisuals = false;
    RenderData::DebugGeometry geometry;
    const ServerViewStats stats = inspector.DrawServerView(kFarAway, depth, geometry);
    EXPECT_TRUE(stats.selectionDrawn);

    const Bounds selected = LinesOfColor(geometry, DebugBatch::Overlay, kSelectedBodyColor);
    EXPECT_EQ(selected.vertices, static_cast<int>(RenderData::kBoxLines * 2));
    EXPECT_NEAR(selected.min.x, 2.0f - kSelectionPaddingM, 1e-3f);
    EXPECT_NEAR(selected.max.x, 4.0f + kSelectionPaddingM, 1e-3f);
    EXPECT_NEAR(selected.min.y, 0.0f - kSelectionPaddingM, 1e-3f);
    EXPECT_NEAR(selected.max.y, 2.0f + kSelectionPaddingM, 1e-3f);

    // Nothing selected, nothing drawn on top of a depth-tested view.
    inspector.Select({});
    geometry.Clear();
    EXPECT_FALSE(inspector.DrawServerView(kFarAway, depth, geometry).selectionDrawn);
    EXPECT_EQ(geometry.VertexCount(DebugBatch::Overlay, DebugPrimitive::Lines), 0u);
}
