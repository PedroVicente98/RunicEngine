// GDB-2 acceptance tests: Godot, headless, draws a RenderFrame's debug geometry and text batched in
// one frame, holds exactly what was drawn, and toggles its layers. Each test starts its own Godot:
// LibGodot allows one instance per process, and CTest runs every test in its own process.

#include "ApplicationHost.hpp"
#include "DebugView.hpp"
#include "GodotLayer.hpp"
#include "GodotRuntime.hpp"
#include "RenderData/Camera.hpp"
#include "RenderData/DebugDraw.hpp"
#include "RenderData/RenderFrameExchange.hpp"

#include <godot_cpp/classes/camera3d.hpp>
#include <godot_cpp/classes/check_box.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/label3d.hpp>
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/window.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

using namespace Runic;
using Application::ApplicationHost;
using GodotBridge::DebugLayer;
using GodotBridge::GodotLayer;
using RenderData::DebugBatch;
using RenderData::DebugPrimitive;

namespace {

constexpr std::uint64_t kFramesToRun = 3;
constexpr float kToleranceM = 1.0e-4f;
const Core::Vec3 kBoxMin(0.0f);
const Core::Vec3 kBoxMax(1.0f, 2.0f, 3.0f);
const Core::Vec3 kLineStart(-1.0f, 0.5f, 0.0f);
const Core::Vec3 kLineEnd(1.0f, 0.5f, 0.0f);
const Core::Vec3 kTriangleA(0.0f, 0.0f, -2.0f);
const Core::Vec3 kTriangleB(1.0f, 0.0f, -2.0f);
const Core::Vec3 kTriangleC(0.0f, 1.0f, -2.0f);
const Core::Vec3 kDepthLabelAt(0.0f, 2.5f, 0.0f);
const Core::Vec3 kOverlayLabelAt(0.0f, 3.0f, 0.0f);
const Core::Vec3 kEye(0.0f, 2.0f, 8.0f);
const Core::Vec3 kTarget(0.0f);
const Core::Vec3 kUp(0.0f, 1.0f, 0.0f);
const Core::Vec3 kLookDirection(0.0f, 0.0f, -1.0f); // a camera looks down its local -Z
constexpr std::string_view kDepthLabel = "engine:test_depth_label";
constexpr std::string_view kOverlayLabel = "engine:test_overlay_label";
constexpr const char *kTestExecutable = "/RunicGodotBridgeTests";

constexpr std::array<DebugBatch, RenderData::kDebugBatchCount> kBatches{DebugBatch::WorldDepth,
                                                                        DebugBatch::Overlay};
constexpr std::array<DebugPrimitive, RenderData::kDebugPrimitiveCount> kPrimitives{
    DebugPrimitive::Lines, DebugPrimitive::Triangles};

// The frame every test draws: a box and a half-transparent triangle in the depth-tested batch, a
// line in the overlay batch, and one label in each.
void DrawTestFrame(RenderData::RenderFrame &frame) {
    frame.debug.Clear();
    frame.camera = RenderData::LookAt(kEye, kTarget, kUp);
    RenderData::DebugDraw depth(frame.debug);
    depth.DrawAABB(kBoxMin, kBoxMax, RenderData::kDebugPhysics);
    depth.FillTriangle(kTriangleA, kTriangleB, kTriangleC,
                       RenderData::WithAlpha(RenderData::kDebugHarmful, RenderData::kHalfAlpha));
    depth.DrawText(kDepthLabelAt, kDepthLabel, RenderData::kDebugText);
    RenderData::DebugDraw overlay(frame.debug, DebugBatch::Overlay);
    overlay.DrawLine(kLineStart, kLineEnd, RenderData::kDebugQuery);
    overlay.DrawText(kOverlayLabelAt, kOverlayLabel, RenderData::kDebugText);
}

class TestFrameWriter final : public Application::Layer {
  public:
    explicit TestFrameWriter(RenderData::RenderFrameExchange &frames)
        : Layer("engine:test_frame_writer"), m_Frames(frames) {}
    void OnUpdate(const Application::FrameTime &) override {
        DrawTestFrame(m_Frames.WriteFrame());
        m_Frames.Publish();
    }

  private:
    RenderData::RenderFrameExchange &m_Frames;
};

bool LibGodotBuilt() {
    return std::filesystem::exists(
        GodotBridge::GodotRuntime::DefaultLibraryPath(GodotBridge::GodotTarget::Editor));
}

// Godot headless on the spike project: the test writer below the GodotLayer, frames run by hand.
class GodotHarness {
  public:
    explicit GodotHarness(bool withFrames) {
        GodotBridge::GodotLayerSpec spec;
        spec.arguments = {GodotBridge::GodotRuntime::ExecutableDirectory() + kTestExecutable,
                          "--headless", "--path", RUNIC_GODOT_SPIKE_PROJECT};
        spec.forwardInput = false;
        spec.frames = withFrames ? &m_Frames : nullptr;
        spec.developmentOverlay = withFrames;
        auto application = std::make_unique<Application::Application>(
            Application::ApplicationSpec{"test", Application::LoopPolicy::PacedByTopLayer(), 0});
        if (withFrames) {
            application->PushLayer(std::make_unique<TestFrameWriter>(m_Frames));
        }
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
    void RunFrames(std::uint64_t count) {
        for (std::uint64_t i = 0; i < count; ++i) {
            m_Host->RunFrame(Application::FrameTime{m_Host->FramesRun()});
        }
    }
    GodotLayer &Layer() { return *m_Layer; }
    std::uint64_t Published() const { return m_Frames.PublishedFrames(); }

  private:
    RenderData::RenderFrameExchange m_Frames;
    GodotLayer *m_Layer = nullptr;
    std::unique_ptr<ApplicationHost> m_Host;
};

godot::String ToGodot(std::string_view text) {
    return godot::String::utf8(text.data(), static_cast<int>(text.size()));
}

godot::Node *FindNode(std::string_view name) {
    auto *tree =
        godot::Object::cast_to<godot::SceneTree>(godot::Engine::get_singleton()->get_main_loop());
    return tree == nullptr ? nullptr : tree->get_root()->find_child(ToGodot(name), true, false);
}

std::vector<godot::Label3D *> VisibleLabels() {
    std::vector<godot::Label3D *> labels;
    if (godot::Node *view = FindNode(GodotBridge::kDebugViewNodeName)) {
        for (int i = 0; i < view->get_child_count(); ++i) {
            auto *label = godot::Object::cast_to<godot::Label3D>(view->get_child(i));
            if (label != nullptr && label->is_visible()) {
                labels.push_back(label);
            }
        }
    }
    return labels;
}

std::uint32_t VerticesOf(const RenderData::DebugGeometry &geometry, DebugPrimitive primitive) {
    std::uint32_t vertices = 0;
    for (const DebugBatch batch : kBatches) {
        vertices += geometry.VertexCount(batch, primitive);
    }
    return vertices;
}

} // namespace

#define RUNIC_REQUIRE_LIBGODOT()                                                                   \
    if (!LibGodotBuilt()) {                                                                        \
        GTEST_SKIP() << "libgodot is not built";                                                   \
    }

TEST(DebugView, GodotsSurfaceLayoutIsDebugGeometrysLayout) {
    RUNIC_REQUIRE_LIBGODOT();
    GodotHarness godot(true);
    ASSERT_TRUE(godot.Start());
    godot.RunFrames(1);
    auto *server = godot::RenderingServer::get_singleton();
    const godot::BitField<godot::RenderingServer::ArrayFormat> format(
        godot::RenderingServer::ARRAY_FORMAT_VERTEX | godot::RenderingServer::ARRAY_FORMAT_COLOR |
        godot::RenderingServer::ARRAY_FLAG_FORMAT_CURRENT_VERSION);
    const auto vertices = static_cast<int32_t>(RenderData::kVerticesPerLine);
    EXPECT_EQ(server->mesh_surface_get_format_vertex_stride(format, vertices), sizeof(Core::Vec3));
    EXPECT_EQ(server->mesh_surface_get_format_attribute_stride(format, vertices),
              sizeof(RenderData::PackedColor));
    EXPECT_EQ(server->mesh_surface_get_format_offset(format, vertices,
                                                     godot::RenderingServer::ARRAY_COLOR),
              0u);
    EXPECT_TRUE(godot.Layer().DebugStats().layoutMatches);
}

TEST(DebugView, WhatGodotHoldsIsWhatWasDrawn) {
    RUNIC_REQUIRE_LIBGODOT();
    GodotHarness godot(true);
    ASSERT_TRUE(godot.Start());
    godot.RunFrames(kFramesToRun);
    RenderData::RenderFrame drawn;
    DrawTestFrame(drawn);
    for (const DebugBatch batch : kBatches) {
        for (const DebugPrimitive primitive : kPrimitives) {
            std::vector<Core::Vec3> positions;
            std::vector<RenderData::PackedColor> colors;
            const bool held =
                godot.Layer().ReadBackDebugSurface(batch, primitive, positions, colors);
            const auto expectedPositions = drawn.debug.Positions(batch, primitive);
            const auto expectedColors = drawn.debug.Colors(batch, primitive);
            ASSERT_EQ(held, !expectedPositions.empty());
            ASSERT_EQ(positions.size(), expectedPositions.size());
            ASSERT_EQ(colors.size(), expectedColors.size());
            for (std::size_t i = 0; i < positions.size(); ++i) {
                EXPECT_EQ(positions[i], expectedPositions[i]);
                EXPECT_EQ(colors[i], expectedColors[i]);
            }
        }
    }
}

TEST(DebugView, EveryShapeAndTextDrawsBatchedInOneFrame) {
    RUNIC_REQUIRE_LIBGODOT();
    GodotHarness godot(true);
    ASSERT_TRUE(godot.Start());
    godot.RunFrames(kFramesToRun);
    RenderData::RenderFrame drawn;
    DrawTestFrame(drawn);
    std::uint32_t nonEmptySurfaces = 0;
    for (const DebugBatch batch : kBatches) {
        for (const DebugPrimitive primitive : kPrimitives) {
            nonEmptySurfaces += drawn.debug.VertexCount(batch, primitive) > 0 ? 1 : 0;
        }
    }
    const GodotBridge::DebugViewStats stats = godot.Layer().DebugStats();
    EXPECT_EQ(stats.frame, godot.Published());
    EXPECT_EQ(stats.surfaces, nonEmptySurfaces) << "one surface per non-empty batch and primitive";
    EXPECT_EQ(stats.lineVertices, VerticesOf(drawn.debug, DebugPrimitive::Lines));
    EXPECT_EQ(stats.triangleVertices, VerticesOf(drawn.debug, DebugPrimitive::Triangles));
    EXPECT_EQ(stats.texts, drawn.debug.Texts().size());
    const std::vector<godot::Label3D *> labels = VisibleLabels();
    ASSERT_EQ(labels.size(), drawn.debug.Texts().size()) << "one label per text";
    for (std::size_t i = 0; i < labels.size(); ++i) {
        EXPECT_EQ(labels[i]->get_text(), ToGodot(drawn.debug.TextOf(drawn.debug.Texts()[i])));
    }
}

TEST(DebugView, LayersToggleFromTheApiAndTheOverlay) {
    RUNIC_REQUIRE_LIBGODOT();
    GodotHarness godot(true);
    ASSERT_TRUE(godot.Start());
    godot.RunFrames(1);
    RenderData::RenderFrame drawn;
    DrawTestFrame(drawn);

    godot.Layer().SetDebugLayerVisible(DebugLayer::Text, false);
    godot.RunFrames(1);
    EXPECT_FALSE(godot.Layer().IsDebugLayerVisible(DebugLayer::Text));
    EXPECT_EQ(godot.Layer().DebugStats().texts, 0u);
    EXPECT_TRUE(VisibleLabels().empty());
    auto *textToggle = godot::Object::cast_to<godot::CheckBox>(
        FindNode(GodotBridge::DebugLayerToggleName(DebugLayer::Text)));
    ASSERT_NE(textToggle, nullptr) << "the development overlay has a checkbox per layer";
    EXPECT_FALSE(textToggle->is_pressed()) << "the overlay follows the API";

    auto *depthToggle = godot::Object::cast_to<godot::CheckBox>(
        FindNode(GodotBridge::DebugLayerToggleName(DebugLayer::DepthShapes)));
    ASSERT_NE(depthToggle, nullptr);
    depthToggle->set_pressed(false); // as a click would
    EXPECT_FALSE(godot.Layer().IsDebugLayerVisible(DebugLayer::DepthShapes));
    godot.RunFrames(1);
    const GodotBridge::DebugViewStats stats = godot.Layer().DebugStats();
    EXPECT_EQ(stats.lineVertices,
              drawn.debug.VertexCount(DebugBatch::Overlay, DebugPrimitive::Lines))
        << "only the overlay batch's lines are left";
    EXPECT_EQ(stats.triangleVertices, 0u);

    depthToggle->set_pressed(true);
    godot.RunFrames(1);
    EXPECT_TRUE(godot.Layer().IsDebugLayerVisible(DebugLayer::DepthShapes));
    EXPECT_EQ(godot.Layer().DebugStats().lineVertices,
              VerticesOf(drawn.debug, DebugPrimitive::Lines));
}

TEST(DebugView, TheCameraFollowsTheFrame) {
    RUNIC_REQUIRE_LIBGODOT();
    GodotHarness godot(true);
    ASSERT_TRUE(godot.Start());
    godot.RunFrames(kFramesToRun);
    auto *camera =
        godot::Object::cast_to<godot::Camera3D>(FindNode(GodotBridge::kDebugCameraNodeName));
    ASSERT_NE(camera, nullptr);
    EXPECT_TRUE(camera->is_current());
    const godot::Transform3D transform = camera->get_global_transform();
    EXPECT_NEAR(transform.origin.x, kEye.x, kToleranceM);
    EXPECT_NEAR(transform.origin.y, kEye.y, kToleranceM);
    EXPECT_NEAR(transform.origin.z, kEye.z, kToleranceM);
    const godot::Vector3 looking =
        transform.basis.xform(godot::Vector3(kLookDirection.x, kLookDirection.y, kLookDirection.z));
    const Core::Vec3 expected = glm::normalize(kTarget - kEye);
    EXPECT_NEAR(looking.x, expected.x, kToleranceM);
    EXPECT_NEAR(looking.y, expected.y, kToleranceM);
    EXPECT_NEAR(looking.z, expected.z, kToleranceM);
    EXPECT_NEAR(camera->get_fov(), RenderData::kDefaultVerticalFovDegrees, kToleranceM);
}

TEST(DebugView, WithoutFramesTheSceneIsUntouched) {
    RUNIC_REQUIRE_LIBGODOT();
    GodotHarness godot(false);
    ASSERT_TRUE(godot.Start());
    godot.RunFrames(kFramesToRun);
    EXPECT_EQ(FindNode(GodotBridge::kDebugViewNodeName), nullptr);
    EXPECT_EQ(FindNode(GodotBridge::kDevelopmentOverlayNodeName), nullptr);
    EXPECT_EQ(godot.Layer().DebugStats().frame, 0u);
    std::vector<Core::Vec3> positions;
    std::vector<RenderData::PackedColor> colors;
    EXPECT_FALSE(godot.Layer().ReadBackDebugSurface(DebugBatch::WorldDepth, DebugPrimitive::Lines,
                                                    positions, colors));
}
