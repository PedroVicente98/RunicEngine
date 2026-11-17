// GDB-4 acceptance tests: the physics geometry layer. Godot, headless, draws the frame's physics
// geometry moved by the presenter's shift, so collision lines up with the gameplay shapes even
// after the presenter's origin moves; cached debug meshes are uploaded once and drawn by instances;
// the layer toggles, and "see what the server sees" draws it on top. Each test starts its own
// Godot.

#include "ApplicationHost.hpp"
#include "GodotLayer.hpp"
#include "GodotPresenter.hpp"
#include "GodotRuntime.hpp"
#include "RenderData/Camera.hpp"
#include "RenderData/DebugDraw.hpp"
#include "RenderData/DebugMeshes.hpp"
#include "RenderData/RenderFrameExchange.hpp"
#include "RenderData/ShapeInstances.hpp"

#include <godot_cpp/classes/check_box.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/window.hpp>

#include <gtest/gtest.h>

#include <filesystem>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

using namespace Runic;
using Core::DebugMeshId;
using Core::RenderInstanceId;
using Core::Vec3;
using Core::WorldPosition;
using GodotBridge::DebugLayer;
using GodotBridge::GodotLayer;
using RenderData::DebugBatch;
using RenderData::DebugPrimitive;

namespace {

constexpr const char *kTestExecutable = "/RunicGodotBridgeTests";
constexpr Core::Quat kNoTurn{1.0f, 0.0f, 0.0f, 0.0f};
const Vec3 kUp(0.0f, 1.0f, 0.0f);
const Vec3 kAhead(0.0f, 0.0f, -10.0f);
// Half a millimetre: the float step at 4 km, the most a rebased presenter lets positions reach.
constexpr float kAlignedM = 0.0005f;
constexpr std::uint64_t kCrateId = 1;
const Vec3 kCrateHalf(0.5f, 0.5f, 0.5f);

// What the test wants in the next frame: the camera, one crate as a gameplay shape and as physics
// geometry (a wire box, as Physics draws one), and cached meshes' instances.
struct Scene {
    WorldPosition eye;
    WorldPosition crate;
    std::vector<RenderData::DebugMeshInstance> instances; // positions relative to the eye
};

class SceneWriter final : public Application::Layer {
  public:
    SceneWriter(RenderData::RenderFrameExchange &frames, Scene &scene)
        : Layer("engine:test_physics_writer"), m_Frames(frames), m_Scene(scene) {}
    void OnUpdate(const Application::FrameTime &) override {
        RenderData::RenderFrame &frame = m_Frames.WriteFrame();
        frame.debug.Clear();
        frame.physics.Clear();
        frame.origin = m_Scene.eye;
        frame.camera = RenderData::LookAt(Vec3(0.0f), kAhead, kUp);
        m_Writer.Begin(frame.shapes, m_Scene.eye);
        m_Writer.Add(RenderInstanceId(kCrateId), RenderData::ShapeKind::Box, m_Scene.crate, kNoTurn,
                     kCrateHalf * 2.0f, RenderData::kDebugPhysics);
        m_Writer.End();
        const Vec3 center = Core::ToLocal(m_Scene.crate - m_Scene.eye);
        RenderData::DebugDraw physics(frame.physics);
        physics.DrawAABB(center - kCrateHalf, center + kCrateHalf, RenderData::kDebugPhysics);
        for (const RenderData::DebugMeshInstance &instance : m_Scene.instances) {
            frame.physics.AddMeshInstance(instance);
        }
        m_Frames.Publish();
    }

  private:
    RenderData::RenderFrameExchange &m_Frames;
    Scene &m_Scene;
    RenderData::ShapeInstanceWriter m_Writer;
};

bool LibGodotBuilt() {
    return std::filesystem::exists(
        GodotBridge::GodotRuntime::DefaultLibraryPath(GodotBridge::GodotTarget::Editor));
}

class GodotHarness {
  public:
    explicit GodotHarness(Scene &scene, bool overlay = false) {
        GodotBridge::GodotLayerSpec spec;
        spec.arguments = {GodotBridge::GodotRuntime::ExecutableDirectory() + kTestExecutable,
                          "--headless", "--path", RUNIC_GODOT_SPIKE_PROJECT};
        spec.forwardInput = false;
        spec.frames = &m_Frames;
        spec.developmentOverlay = overlay;
        auto application = std::make_unique<Application::Application>(
            Application::ApplicationSpec{"test", Application::LoopPolicy::PacedByTopLayer(), 0});
        application->PushLayer(std::make_unique<SceneWriter>(m_Frames, scene));
        auto layer = std::make_unique<GodotLayer>(std::move(spec));
        m_Layer = layer.get();
        application->PushLayer(std::move(layer));
        m_Host = std::make_unique<Application::ApplicationHost>(
            std::move(application), Application::CommandLine({kTestExecutable}));
    }
    ~GodotHarness() { m_Host->Shutdown(); }
    GodotHarness(const GodotHarness &) = delete;
    GodotHarness &operator=(const GodotHarness &) = delete;

    bool Start() { return static_cast<bool>(m_Host->Start()); }
    void RunFrame() { m_Host->RunFrame(Application::FrameTime{m_Host->FramesRun()}); }
    GodotLayer &Layer() { return *m_Layer; }
    RenderData::DebugMeshLibrary &Meshes() { return m_Frames.Meshes(); }

  private:
    RenderData::RenderFrameExchange m_Frames;
    GodotLayer *m_Layer = nullptr;
    std::unique_ptr<Application::ApplicationHost> m_Host;
};

// The center of what the physics layer's depth-tested lines span, in Godot's space.
bool PhysicsCenter(GodotLayer &layer, Vec3 &center) {
    std::vector<Vec3> positions;
    std::vector<RenderData::PackedColor> colors;
    if (!layer.ReadBackPhysicsSurface(DebugBatch::WorldDepth, DebugPrimitive::Lines, positions,
                                      colors) ||
        positions.empty()) {
        return false;
    }
    Vec3 low(std::numeric_limits<float>::max());
    Vec3 high(std::numeric_limits<float>::lowest());
    for (const Vec3 &p : positions) {
        low = glm::min(low, p);
        high = glm::max(high, p);
    }
    center = (low + high) * 0.5f;
    return true;
}

std::shared_ptr<const RenderData::DebugMesh> Fan(float size) {
    auto mesh = std::make_shared<RenderData::DebugMesh>();
    mesh->trianglePositions = {Vec3(0.0f), Vec3(size, 0.0f, 0.0f), Vec3(0.0f, 0.0f, size)};
    mesh->triangleColors = {RenderData::kDebugPhysics, RenderData::kDebugPhysics,
                            RenderData::kDebugPhysics};
    mesh->linePositions = {Vec3(0.0f), Vec3(size, 0.0f, 0.0f)};
    mesh->lineColors = {RenderData::kDebugInactive, RenderData::kDebugInactive};
    return mesh;
}

} // namespace

#define RUNIC_REQUIRE_LIBGODOT()                                                                   \
    if (!LibGodotBuilt()) {                                                                        \
        GTEST_SKIP() << "libgodot is not built";                                                   \
    }

TEST(PhysicsLayer, CollisionLinesUpWithGameplayShapesAcrossOriginMoves) {
    RUNIC_REQUIRE_LIBGODOT();
    // The camera walks 9 km far from the world's origin, so the presenter's origin moves twice; the
    // crate stays 3 m ahead of it. Its physics wire box and its gameplay shape stay together.
    const WorldPosition start{5'000'000.123, 120.456, -7'000'000.789};
    const Vec3 ahead(0.4f, -1.0f, -3.0f);
    Scene scene;
    scene.eye = start;
    scene.crate = start + ahead;
    GodotHarness godot(scene);
    ASSERT_TRUE(godot.Start());
    float worst = 0.0f;
    constexpr int kSteps = 90;
    constexpr double kStepM = 100.0;
    for (int step = 0; step <= kSteps; ++step) {
        scene.eye.x = start.x + step * kStepM;
        scene.crate = scene.eye + ahead;
        godot.RunFrame();
        GodotBridge::PresentedInstance shape;
        ASSERT_TRUE(godot.Layer().ReadBackInstance(RenderInstanceId(kCrateId), shape));
        Vec3 collision;
        ASSERT_TRUE(PhysicsCenter(godot.Layer(), collision));
        worst = std::max(worst, glm::length(collision - shape.position));
    }
    EXPECT_LT(worst, kAlignedM);
    EXPECT_EQ(godot.Layer().PresenterReport().rebases, 2u); // at 4,100 and 8,200 m
    EXPECT_GT(godot.Layer().DebugStats().physicsLineVertices, 0u);
    RecordProperty("worst_mm", std::to_string(worst * 1000.0f));
}

TEST(PhysicsLayer, CachedMeshesUploadOnceAndFollowTheirInstances) {
    RUNIC_REQUIRE_LIBGODOT();
    Scene scene;
    scene.eye = {1'000.0, 0.0, 1'000.0};
    scene.crate = scene.eye + Vec3(0.0f, 0.0f, -3.0f);
    GodotHarness godot(scene);
    godot.Meshes().Put(DebugMeshId(7), Fan(2.0f));
    scene.instances.push_back(
        {DebugMeshId(7), DebugBatch::WorldDepth, Vec3(1.0f, 0.0f, -4.0f), kNoTurn});
    scene.instances.push_back(
        {DebugMeshId(7), DebugBatch::Overlay, Vec3(-1.0f, 0.0f, -4.0f), kNoTurn});
    ASSERT_TRUE(godot.Start());
    for (int frame = 0; frame < 10; ++frame) {
        godot.RunFrame();
    }
    GodotBridge::DebugViewStats stats = godot.Layer().DebugStats();
    EXPECT_EQ(stats.cachedMeshes, 1u);
    EXPECT_EQ(stats.meshUploads, 1u); // once, however many frames draw it
    EXPECT_EQ(stats.meshInstances, 2u);
    DebugMeshId mesh;
    Vec3 first;
    Vec3 second;
    ASSERT_TRUE(godot.Layer().ReadBackMeshInstance(0, mesh, first));
    ASSERT_TRUE(godot.Layer().ReadBackMeshInstance(1, mesh, second));
    EXPECT_FALSE(godot.Layer().ReadBackMeshInstance(2, mesh, second));
    EXPECT_EQ(mesh, DebugMeshId(7));
    EXPECT_NEAR(first.x - second.x, 2.0f, 1e-5f);

    // Moved: the instance follows; replaced: one more upload; removed and no longer listed: gone.
    scene.instances.resize(1);
    scene.instances[0].position = Vec3(5.0f, 0.0f, -4.0f);
    godot.Meshes().Put(DebugMeshId(7), Fan(3.0f));
    godot.RunFrame();
    stats = godot.Layer().DebugStats();
    EXPECT_EQ(stats.meshUploads, 2u);
    EXPECT_EQ(stats.meshInstances, 1u);
    ASSERT_TRUE(godot.Layer().ReadBackMeshInstance(0, mesh, second));
    EXPECT_NEAR(second.x - first.x, 4.0f, 1e-5f);
    scene.instances.clear();
    godot.Meshes().Remove(DebugMeshId(7));
    godot.RunFrame();
    stats = godot.Layer().DebugStats();
    EXPECT_EQ(stats.cachedMeshes, 0u);
    EXPECT_EQ(stats.meshInstances, 0u);
}

TEST(PhysicsLayer, TogglesAndDrawsOnTopToSeeWhatTheServerSees) {
    RUNIC_REQUIRE_LIBGODOT();
    Scene scene;
    scene.eye = {0.0, 0.0, 0.0};
    scene.crate = {0.0, 0.0, -3.0};
    GodotHarness godot(scene, true);
    ASSERT_TRUE(godot.Start());
    godot.RunFrame();
    GodotLayer &layer = godot.Layer();
    EXPECT_TRUE(layer.IsDebugLayerVisible(DebugLayer::PhysicsGeometry));
    EXPECT_FALSE(layer.IsDebugLayerVisible(DebugLayer::ServerView));
    EXPECT_FALSE(layer.PhysicsDrawnOnTop());
    EXPECT_GT(layer.DebugStats().physicsLineVertices, 0u);

    layer.SetDebugLayerVisible(DebugLayer::ServerView, true);
    godot.RunFrame();
    EXPECT_TRUE(layer.PhysicsDrawnOnTop());
    Vec3 center;
    EXPECT_TRUE(PhysicsCenter(layer, center)); // still drawn, now over everything

    // The overlay has a check box for each, and pressing one toggles its layer.
    auto *tree =
        godot::Object::cast_to<godot::SceneTree>(godot::Engine::get_singleton()->get_main_loop());
    auto *toggle = godot::Object::cast_to<godot::CheckBox>(tree->get_root()->find_child(
        godot::String(
            std::string(GodotBridge::DebugLayerToggleName(DebugLayer::PhysicsGeometry)).c_str()),
        true, false));
    ASSERT_NE(toggle, nullptr);
    EXPECT_TRUE(toggle->is_pressed());
    toggle->set_pressed(false);
    godot.RunFrame();
    EXPECT_FALSE(layer.IsDebugLayerVisible(DebugLayer::PhysicsGeometry));
    EXPECT_EQ(layer.DebugStats().physicsLineVertices, 0u);
    EXPECT_FALSE(PhysicsCenter(layer, center));
}
