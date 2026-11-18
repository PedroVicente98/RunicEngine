// GDT-3 with Godot: the physics inspector in the editor, headless, in this process. The editor's
// local simulation runs physics thousands of kilometres from the world's origin, where the edited
// scene stands; the Runic Physics dock lists the bodies near the editor's camera, selects one in
// its list or with a ray (what the plugin does for a click in a 3D viewport) and shows its
// properties; the "see what the server sees" layer draws the collision into the world the editor's
// viewports render, at the edited scene's place, over the visuals or depth-tested.

#include "EditorHarness.hpp"
#include "PhysicsBridge.hpp"
#include "PhysicsInspector.hpp"
#include "WorldComponents.hpp"

#include <godot_cpp/classes/check_box.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/window.hpp>
#include <godot_cpp/variant/packed_vector3_array.hpp>
#include <godot_cpp/variant/vector3.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <limits>
#include <string>

using namespace Runic;
using Core::Vec3;
using Core::WorldPosition;
using GodotTools::Testing::EditorHarness;
using GodotTools::Testing::ScratchProject;

namespace {

const WorldPosition kSceneOrigin{5'000'000.0, 120.0, -7'000'000.0};
constexpr const char *kDock = "Runic Physics";
// RenderData's numbers for the batches and primitives the check methods take.
constexpr int64_t kWorldDepth = 0;
constexpr int64_t kOverlay = 1;
constexpr int64_t kLines = 0;
constexpr float kToleranceM = 1e-3f;

WorldPosition InScene(double x, double y, double z) {
    return {kSceneOrigin.x + x, kSceneOrigin.y + y, kSceneOrigin.z + z};
}

void RunRealtimeTick(Simulation::SimulationRuntime &runtime) {
    const std::uint64_t before = runtime.TicksRun(Simulation::TickDomain::Hz30);
    while (runtime.TicksRun(Simulation::TickDomain::Hz30) == before) {
        runtime.RunMasterTick();
    }
}

// The editor's local simulation: a 20 m static floor whose top is the scene's ground, a 2 m
// kinematic box at (3, 1, -2) and a static 1 m sphere at (-4, 1, 0), each an entity's body.
class LocalWorld {
  public:
    LocalWorld() {
        Add("engine:test_floor", Physics::BoxShape(Vec3(10.0f, 0.5f, 10.0f)),
            Physics::MotionType::Static, InScene(0.0, -0.5, 0.0));
        Add("engine:test_box", Physics::BoxShape(Vec3(1.0f)), Physics::MotionType::Kinematic,
            InScene(3.0, 1.0, -2.0));
        Add("engine:test_sphere", Physics::SphereShape(0.5f), Physics::MotionType::Static,
            InScene(-4.0, 1.0, 0.0));
        RunRealtimeTick(m_Runtime);
    }
    Simulation::SimulationRuntime &Runtime() { return m_Runtime; }
    // Whether this build draws physics at all (RUNIC_PHYSICS_DEBUG_VIEW).
    bool HasDebugView() {
        RenderData::DebugGeometry geometry;
        return GodotTools::PhysicsInspector(m_Runtime)
            .DrawServerView(kSceneOrigin, {}, geometry)
            .physics.available;
    }

  private:
    void Add(std::string_view name, const Physics::ShapeDesc &shape, Physics::MotionType motion,
             WorldPosition where) {
        Physics::PhysicsWorld &world = m_Bridge.World();
        Physics::BodyDesc body;
        body.shape = world.AddShape(name, shape).Value();
        body.motion = motion;
        body.layer =
            world.Layers().Find(motion == Physics::MotionType::Static ? Physics::kStaticLayerName
                                                                      : Physics::kMovingLayerName);
        const ECS::Entity entity = m_Runtime.Entities().Create();
        m_Runtime.Entities().Set(entity, World::WorldTransform{where});
        m_Bridge.AddBody(entity, body);
    }

    Simulation::SimulationRuntime m_Runtime;
    Simulation::PhysicsBridge &m_Bridge = m_Runtime.EnablePhysics();
};

godot::Node *PhysicsDock() {
    godot::Engine *engine = godot::Engine::get_singleton();
    auto *tree = engine != nullptr
                     ? godot::Object::cast_to<godot::SceneTree>(engine->get_main_loop())
                     : nullptr;
    if (tree == nullptr || tree->get_root() == nullptr) {
        return nullptr;
    }
    return tree->get_root()->find_child(kDock, true, false);
}

int64_t ShownBodies() {
    godot::Node *dock = PhysicsDock();
    return dock != nullptr ? static_cast<int64_t>(dock->call("shown_bodies")) : 0;
}

std::string Property(godot::Node *dock, const char *name) {
    const godot::String value = dock->call("selected_property", name);
    return value.utf8().get_data();
}

godot::PackedVector3Array Surface(godot::Node *dock, int64_t batch) {
    return dock->call("server_view_surface", batch, kLines);
}

struct Bounds {
    Vec3 min{std::numeric_limits<float>::max()};
    Vec3 max{std::numeric_limits<float>::lowest()};
};

Bounds BoundsOf(const godot::PackedVector3Array &positions) {
    Bounds bounds;
    for (int64_t i = 0; i < positions.size(); ++i) {
        const godot::Vector3 p = positions[i];
        bounds.min = glm::min(bounds.min, Vec3(p.x, p.y, p.z));
        bounds.max = glm::max(bounds.max, Vec3(p.x, p.y, p.z));
    }
    return bounds;
}

void Toggle(godot::Node *dock, const char *name, bool pressed) {
    auto *box = godot::Object::cast_to<godot::CheckBox>(dock->find_child(name, true, false));
    ASSERT_NE(box, nullptr) << name;
    box->set_pressed(pressed); // emits toggled, as a click does
}

} // namespace

TEST(RunicPhysicsDock, ListsTheLocalSimulationsBodiesAndShowsTheSelectedOnesProperties) {
    RUNIC_REQUIRE_LIBGODOT();
    ScratchProject project("physics-dock");
    ASSERT_TRUE(project.Made());
    LocalWorld world;
    EditorHarness editor(project.Path(), world.Runtime(), kSceneOrigin);
    ASSERT_TRUE(editor.Start());
    ASSERT_TRUE(editor.RunUntil([] { return ShownBodies() == 3; }));
    godot::Node *dock = PhysicsDock();

    // In the list, in handle order: the floor, the box, the sphere.
    EXPECT_EQ(Property(dock, "shape"), "");
    ASSERT_TRUE(static_cast<bool>(dock->call("select_body", 1)));
    EXPECT_EQ(Property(dock, "shape"), "engine:test_box");
    EXPECT_EQ(Property(dock, "motion"), "kinematic");
    EXPECT_EQ(Property(dock, "position"), "5000003.000, 121.000, -7000002.000");
    EXPECT_EQ(Property(dock, "owner").rfind("entity ", 0), 0u);

    // A ray in the scene's space, as the plugin casts one from a click: the sphere, then nothing.
    EXPECT_TRUE(static_cast<bool>(
        dock->call("pick_ray", godot::Vector3(-4.0f, 20.0f, 0.0f), godot::Vector3(0, -1, 0))));
    EXPECT_EQ(Property(dock, "shape"), "engine:test_sphere");
    EXPECT_EQ(Property(dock, "kind"), "sphere");
    EXPECT_FALSE(static_cast<bool>(
        dock->call("pick_ray", godot::Vector3(50.0f, 20.0f, 50.0f), godot::Vector3(0, -1, 0))));
    EXPECT_EQ(Property(dock, "shape"), "");
}

TEST(RunicPhysicsDock, TheServerViewDrawsTheCollisionAtTheScenesPlaceOverTheVisuals) {
    RUNIC_REQUIRE_LIBGODOT();
    LocalWorld world;
    if (!world.HasDebugView()) {
        GTEST_SKIP() << "this build has no physics debug view";
    }
    ScratchProject project("server-view");
    ASSERT_TRUE(project.Made());
    EditorHarness editor(project.Path(), world.Runtime(), kSceneOrigin);
    ASSERT_TRUE(editor.Start());
    ASSERT_TRUE(editor.RunUntil([] {
        godot::Node *dock = PhysicsDock();
        return dock != nullptr && Surface(dock, kOverlay).size() > 0;
    }));
    godot::Node *dock = PhysicsDock();
    EXPECT_TRUE(static_cast<bool>(dock->call("server_view_shown")));
    EXPECT_TRUE(static_cast<bool>(dock->call("server_view_drawn_over_visuals")));
    EXPECT_EQ(Surface(dock, kWorldDepth).size(), 0);

    // In the scene's space, whatever 16 m cell the editor's camera is in: the floor spans 20 m
    // around the origin under the ground, the box reaches 2 m up.
    const Bounds drawn = BoundsOf(Surface(dock, kOverlay));
    EXPECT_NEAR(drawn.min.x, -10.0f, kToleranceM);
    EXPECT_NEAR(drawn.max.x, 10.0f, kToleranceM);
    EXPECT_NEAR(drawn.min.y, -1.0f, kToleranceM);
    EXPECT_NEAR(drawn.max.y, 2.0f, kToleranceM);
    EXPECT_NEAR(drawn.min.z, -10.0f, kToleranceM);
    EXPECT_NEAR(drawn.max.z, 10.0f, kToleranceM);

    // Depth-tested, the same lines move to the other batch.
    Toggle(dock, "OverVisuals", false);
    ASSERT_TRUE(editor.RunUntil([dock] { return Surface(dock, kWorldDepth).size() > 0; }));
    EXPECT_EQ(Surface(dock, kOverlay).size(), 0);
    EXPECT_FALSE(static_cast<bool>(dock->call("server_view_drawn_over_visuals")));

    Toggle(dock, "ServerView", false);
    EXPECT_FALSE(static_cast<bool>(dock->call("server_view_shown")));
}
