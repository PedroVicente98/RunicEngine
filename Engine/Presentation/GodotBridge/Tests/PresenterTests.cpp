// GDB-3 acceptance tests: Godot, headless, keeps the RenderFrame's shape instances in the
// RenderingServer by RenderInstanceId; a frame where nothing changed costs no call; the presenter's
// origin moves rarely and a long walk keeps millimetre precision. Each test starts its own Godot:
// LibGodot allows one instance per process, and CTest runs every test in its own process.

#include "ApplicationHost.hpp"
#include "GodotLayer.hpp"
#include "GodotPresenter.hpp"
#include "GodotRuntime.hpp"
#include "RenderData/Camera.hpp"
#include "RenderData/RenderFrameExchange.hpp"
#include "RenderData/ShapeInstances.hpp"

#include <godot_cpp/classes/camera3d.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/window.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using namespace Runic;
using Application::ApplicationHost;
using Core::RenderInstanceId;
using Core::Vec3;
using Core::WorldPosition;
using GodotBridge::GodotLayer;
using GodotBridge::PresentedInstance;
using GodotBridge::PresenterStats;
using RenderData::ShapeKind;

namespace {

constexpr const char *kTestExecutable = "/RunicGodotBridgeTests";
constexpr Core::Quat kNoTurn{1.0f, 0.0f, 0.0f, 0.0f};
const Vec3 kUp(0.0f, 1.0f, 0.0f);
const Vec3 kAhead(0.0f, 0.0f, -10.0f);
// Half a millimetre: the float step at 4 km, the most a rebased presenter lets positions reach
// near the camera.
constexpr float kNoJitterM = 0.0005f;

// A shape as a test places it in the world.
struct Placed {
    std::uint64_t id = 0;
    WorldPosition position;
    ShapeKind shape = ShapeKind::Box;
    Vec3 size{1.0f};
    RenderData::PackedColor color = RenderData::kDebugInactive;
};

// What the test wants in the next frame: where the camera is and which shapes there are.
struct Scene {
    WorldPosition eye;
    std::vector<Placed> shapes;
    // Frames to publish per update: more than one makes the GodotLayer skip frames.
    int publishes = 1;
};

// Below the GodotLayer: writes the test's scene into each frame through a ShapeInstanceWriter, as
// an extractor does, with the frame's origin at the camera's eye.
class SceneWriter final : public Application::Layer {
  public:
    SceneWriter(RenderData::RenderFrameExchange &frames, Scene &scene)
        : Layer("engine:test_scene_writer"), m_Frames(frames), m_Scene(scene) {}
    void OnUpdate(const Application::FrameTime &) override {
        for (int publish = 0; publish < m_Scene.publishes; ++publish) {
            RenderData::RenderFrame &frame = m_Frames.WriteFrame();
            frame.debug.Clear();
            frame.origin = m_Scene.eye;
            frame.camera = RenderData::LookAt(Vec3(0.0f), kAhead, kUp);
            m_Writer.Begin(frame.shapes, m_Scene.eye);
            for (const Placed &placed : m_Scene.shapes) {
                m_Writer.Add(RenderInstanceId(placed.id), placed.shape, placed.position, kNoTurn,
                             placed.size, placed.color);
            }
            m_Writer.End();
            m_Frames.Publish();
        }
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

// Godot headless on the spike project, the scene writer below the GodotLayer, frames run by hand.
class GodotHarness {
  public:
    explicit GodotHarness(Scene &scene) {
        GodotBridge::GodotLayerSpec spec;
        spec.arguments = {GodotBridge::GodotRuntime::ExecutableDirectory() + kTestExecutable,
                          "--headless", "--path", RUNIC_GODOT_SPIKE_PROJECT};
        spec.forwardInput = false;
        spec.frames = &m_Frames;
        auto application = std::make_unique<Application::Application>(
            Application::ApplicationSpec{"test", Application::LoopPolicy::PacedByTopLayer(), 0});
        application->PushLayer(std::make_unique<SceneWriter>(m_Frames, scene));
        auto layer = std::make_unique<GodotLayer>(std::move(spec));
        m_Layer = layer.get();
        application->PushLayer(std::move(layer));
        m_Host = std::make_unique<ApplicationHost>(std::move(application),
                                                   Application::CommandLine({kTestExecutable}));
    }
    ~GodotHarness() { m_Host->Shutdown(); }
    GodotHarness(const GodotHarness &) = delete;
    GodotHarness &operator=(const GodotHarness &) = delete;

    bool Start() { return static_cast<bool>(m_Host->Start()); }
    // One frame: the writer publishes, the GodotLayer applies, Godot runs once.
    PresenterStats RunFrame() {
        m_Host->RunFrame(Application::FrameTime{m_Host->FramesRun()});
        return m_Layer->PresenterReport();
    }
    GodotLayer &Layer() { return *m_Layer; }

  private:
    RenderData::RenderFrameExchange m_Frames;
    GodotLayer *m_Layer = nullptr;
    std::unique_ptr<ApplicationHost> m_Host;
};

// The debug camera's place in Godot's space, read back from Godot.
Vec3 GodotCameraPosition() {
    auto *tree =
        godot::Object::cast_to<godot::SceneTree>(godot::Engine::get_singleton()->get_main_loop());
    auto *camera = godot::Object::cast_to<godot::Camera3D>(tree->get_root()->find_child(
        godot::String(std::string(GodotBridge::kDebugCameraNodeName).c_str()), true, false));
    const godot::Vector3 origin = camera->get_global_transform().origin;
    return {origin.x, origin.y, origin.z};
}

PresentedInstance ReadBack(GodotHarness &godot, std::uint64_t id) {
    PresentedInstance instance;
    EXPECT_TRUE(godot.Layer().ReadBackInstance(RenderInstanceId(id), instance)) << "shape " << id;
    return instance;
}

// A grid of still boxes, 2 m apart, around the world's origin.
std::vector<Placed> Grid(std::uint64_t count) {
    constexpr std::uint64_t kRow = 250;
    constexpr double kSpacingM = 2.0;
    std::vector<Placed> shapes;
    for (std::uint64_t i = 0; i < count; ++i) {
        shapes.push_back({i + 1,
                          {static_cast<double>(i % kRow) * kSpacingM, 0.0,
                           -static_cast<double>(i / kRow) * kSpacingM}});
    }
    return shapes;
}

} // namespace

#define RUNIC_REQUIRE_LIBGODOT()                                                                   \
    if (!LibGodotBuilt()) {                                                                        \
        GTEST_SKIP() << "libgodot is not built";                                                   \
    }

TEST(Presenter, MakesOneGodotInstancePerShapeAtItsSize) {
    RUNIC_REQUIRE_LIBGODOT();
    Scene scene;
    scene.eye = {1'000'000.0, 2.0, 1'000'000.0};
    scene.shapes = {
        {1, {1'000'003.0, 0.0, 1'000'000.0}, ShapeKind::Box, Vec3(2.0f, 3.0f, 4.0f)},
        {2, {1'000'000.0, 1.0, 999'990.0}, ShapeKind::Sphere, Vec3(1.5f)},
        {3, {999'998.0, 0.9, 1'000'000.0}, ShapeKind::Capsule, Vec3(0.8f, 1.8f, 0.8f)},
        {4,
         {1'000'000.0, 0.0, 1'000'004.0},
         ShapeKind::Cylinder,
         Vec3(1.0f, 3.0f, 1.0f),
         RenderData::WithAlpha(RenderData::kDebugHarmful, RenderData::kHalfAlpha)},
    };
    GodotHarness godot(scene);
    ASSERT_TRUE(godot.Start());
    const PresenterStats stats = godot.RunFrame();
    EXPECT_EQ(stats.instances, 4u);
    EXPECT_EQ(stats.created, 4u);
    EXPECT_EQ(stats.origin, scene.eye); // the first frame's origin becomes the presenter's

    const PresentedInstance box = ReadBack(godot, 1);
    EXPECT_EQ(box.position, Vec3(3.0f, -2.0f, 0.0f));
    EXPECT_EQ(box.scale, Vec3(2.0f, 3.0f, 4.0f)); // a unit box, scaled
    EXPECT_EQ(ReadBack(godot, 2).scale, Vec3(1.5f));
    const PresentedInstance capsule = ReadBack(godot, 3);
    EXPECT_EQ(capsule.shape, ShapeKind::Capsule);
    EXPECT_EQ(capsule.scale, Vec3(1.0f)); // a mesh of its own size: its ends stay round
    EXPECT_EQ(ReadBack(godot, 4).color,
              RenderData::WithAlpha(RenderData::kDebugHarmful, RenderData::kHalfAlpha));
}

TEST(Presenter, FiftyThousandStillShapesCostNothingPerFrame) {
    RUNIC_REQUIRE_LIBGODOT();
    constexpr std::uint64_t kShapes = 50'000;
    Scene scene;
    scene.eye = {250.0, 30.0, 50.0};
    scene.shapes = Grid(kShapes);
    GodotHarness godot(scene);
    ASSERT_TRUE(godot.Start());
    PresenterStats stats = godot.RunFrame();
    ASSERT_EQ(stats.created, kShapes);
    RecordProperty(
        "create_ms",
        std::to_string(std::chrono::duration<double, std::milli>(stats.applyTime).count()));

    // The camera keeps moving, so every frame has a new origin; the shapes do not.
    constexpr int kFrames = 30;
    Core::Duration slowest{};
    for (int frame = 1; frame <= kFrames; ++frame) {
        scene.eye.x += 1.0;
        stats = godot.RunFrame();
        EXPECT_EQ(stats.serverCalls, 0u) << "frame " << frame;
        EXPECT_EQ(stats.created + stats.updated + stats.freed, 0u) << "frame " << frame;
        slowest = std::max(slowest, stats.applyTime);
    }
    EXPECT_EQ(stats.instances, kShapes);
    RecordProperty("still_frame_us",
                   std::to_string(std::chrono::duration<double, std::micro>(slowest).count()));
}

TEST(Presenter, MovesOnlyTheShapesThatChanged) {
    RUNIC_REQUIRE_LIBGODOT();
    Scene scene;
    scene.shapes = Grid(1000);
    GodotHarness godot(scene);
    ASSERT_TRUE(godot.Start());
    godot.RunFrame();

    for (const std::size_t moved : {10u, 500u, 999u}) {
        scene.shapes[moved].position.y += 0.25;
    }
    PresenterStats stats = godot.RunFrame();
    EXPECT_FALSE(stats.comparedById); // the frame's changed list was enough
    EXPECT_EQ(stats.updated, 3u);
    EXPECT_EQ(stats.serverCalls, 3u); // one transform each
    EXPECT_EQ(ReadBack(godot, 501).position.y, 0.25f);

    scene.shapes[7].color = RenderData::kDebugBeneficial; // recolored only
    stats = godot.RunFrame();
    EXPECT_EQ(stats.updated, 1u);
    EXPECT_EQ(stats.serverCalls, 1u); // its material, not its transform
    EXPECT_EQ(ReadBack(godot, 8).color, RenderData::kDebugBeneficial);
}

TEST(Presenter, FreesWhatLeavesTheFrameAndMakesWhatArrives) {
    RUNIC_REQUIRE_LIBGODOT();
    Scene scene;
    scene.shapes = Grid(100);
    GodotHarness godot(scene);
    ASSERT_TRUE(godot.Start());
    godot.RunFrame();

    scene.shapes.erase(scene.shapes.begin() + 20, scene.shapes.begin() + 22); // shapes 21, 22
    scene.shapes.push_back({1000, {0.0, 5.0, 0.0}, ShapeKind::Sphere});
    const PresenterStats stats = godot.RunFrame();
    EXPECT_TRUE(stats.comparedById);
    EXPECT_EQ(stats.freed, 2u);
    EXPECT_EQ(stats.created, 1u);
    EXPECT_EQ(stats.updated, 0u); // the shapes that stayed did not move
    EXPECT_EQ(stats.instances, 99u);
    PresentedInstance gone;
    EXPECT_FALSE(godot.Layer().ReadBackInstance(RenderInstanceId(21), gone));
    EXPECT_EQ(ReadBack(godot, 1000).position, Vec3(0.0f, 5.0f, 0.0f));
}

TEST(Presenter, AFrameItSkippedIsCaughtUpById) {
    RUNIC_REQUIRE_LIBGODOT();
    Scene scene;
    scene.shapes = Grid(100);
    GodotHarness godot(scene);
    ASSERT_TRUE(godot.Start());
    godot.RunFrame();

    scene.publishes = 2; // the second frame's changed list is relative to one Godot never saw
    scene.shapes[5].position.x += 1.0;
    const PresenterStats stats = godot.RunFrame();
    EXPECT_TRUE(stats.comparedById);
    EXPECT_EQ(stats.updated, 1u);
    EXPECT_EQ(ReadBack(godot, 6).position.x, static_cast<float>(scene.shapes[5].position.x));
}

TEST(Presenter, ATwentyKilometreWalkShowsNoJitter) {
    RUNIC_REQUIRE_LIBGODOT();
    // The camera walks 20 km along +X, 50 m per frame, far from the world's origin; a shape
    // follows it 5 m ahead and 1 m below, and a still marker stands every 100 m along the way.
    constexpr double kWalkM = 20'000.0;
    constexpr double kStepM = 50.0;
    constexpr double kMarkerSpacingM = 100.0;
    // Fractional places, so every conversion to a float rounds.
    const WorldPosition start{-3'000'000.123456, 10.789, 7'000'000.987654};
    const Vec3 followerOffset(0.31f, -1.07f, -5.13f);
    constexpr double kMarkerShiftM = 0.0371;
    constexpr std::uint64_t kFollower = 1;
    constexpr std::uint64_t kFirstMarker = 2;

    Scene scene;
    scene.eye = start;
    scene.shapes.push_back({kFollower, start + followerOffset});
    const auto markers = static_cast<std::uint64_t>(kWalkM / kMarkerSpacingM);
    for (std::uint64_t i = 0; i <= markers; ++i) {
        scene.shapes.push_back({kFirstMarker + i,
                                {start.x + static_cast<double>(i) * kMarkerSpacingM + kMarkerShiftM,
                                 0.0, start.z - 3.0}});
    }
    GodotHarness godot(scene);
    ASSERT_TRUE(godot.Start());
    godot.RunFrame();

    float worstFollower = 0.0f;
    float worstMarker = 0.0f;
    float farthestCamera = 0.0f;
    const auto steps = static_cast<int>(kWalkM / kStepM);
    for (int step = 1; step <= steps; ++step) {
        scene.eye.x = start.x + static_cast<double>(step) * kStepM;
        scene.shapes[0].position = scene.eye + followerOffset;
        const PresenterStats stats = godot.RunFrame();
        // Markers more than 10 km away do not fit a frame (Core::kMaxLocalOffsetM): the rest do.
        std::uint32_t inReach = 0;
        for (const Placed &placed : scene.shapes) {
            inReach += std::abs(placed.position.x - scene.eye.x) < Core::kMaxLocalOffsetM ? 1 : 0;
        }
        ASSERT_EQ(stats.instances, inReach);

        const Vec3 camera = GodotCameraPosition();
        farthestCamera = std::max(farthestCamera, glm::length(camera));
        // What the renderer sees: positions relative to the camera, in Godot's floats.
        const Vec3 follower = ReadBack(godot, kFollower).position - camera;
        worstFollower = std::max(worstFollower, glm::length(follower - followerOffset));
        // The marker just ahead of the camera, against its exact place.
        const auto nearest = static_cast<std::uint64_t>(
            std::ceil(static_cast<double>(step) * kStepM / kMarkerSpacingM));
        const Placed &marker = scene.shapes[1 + std::min(nearest, markers)];
        const Vec3 expected = Core::ToLocal(marker.position - scene.eye);
        const Vec3 seen = ReadBack(godot, marker.id).position - camera;
        worstMarker = std::max(worstMarker, glm::length(seen - expected));
    }
    const PresenterStats stats = godot.Layer().PresenterReport();
    EXPECT_LT(worstFollower, kNoJitterM);
    EXPECT_LT(worstMarker, kNoJitterM);
    // Godot's coordinates stay within the rebase distance. The origin moved at the first step past
    // 4 km each time: at 4,050, 8,100, 12,150 and 16,200 m.
    constexpr std::uint64_t kRebasesOnTheWalk = 4;
    EXPECT_LE(farthestCamera, static_cast<float>(GodotBridge::kRebaseDistanceM));
    EXPECT_EQ(stats.rebases, kRebasesOnTheWalk);
    RecordProperty("worst_follower_mm", std::to_string(worstFollower * 1000.0f));
    RecordProperty("worst_marker_mm", std::to_string(worstMarker * 1000.0f));
}
