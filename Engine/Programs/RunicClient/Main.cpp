// RunicClient: the Engine's own game-agnostic client (RunicDocs › Repositories, build targets and
// processes › Processes): the Engine's client layers with the GodotLayer on top, on a neutral test
// project. Engine slices show their client results here; it is never shipped.
//
// The stack, bottom to top: the input trace, the Simulation layer (a local simulation), the Input
// layer (slice 7: keys and clicks become actions, and command builders turn them into commands),
// the scene, the Presentation layer (the camera and the extractors) and the GodotLayer, which
// draws only the frames the Presentation layer publishes. Two scenes:
//   - the walk (the default, slice 7): a capsule walks on static terrain 8,600 km from the origin
//   under
//     keyboard control (WalkScene.hpp has the keys), with the camera behind it;
//   - the moving entities (slice 5, --moving-scene): walkers steered by commands around slice 2's
//   debug scene, the camera
//     circling them.
//
//   RunicClient [--moving-scene]
//       a window with the test project; keys and clicks that reach the stack are printed, and a
//       click on the test button is not (Godot's UI keeps it)
//   RunicClient --headless --quit-after 60
//       Godot starts headless and the client exits cleanly (the CTest godot.runic_client_headless)
//   RunicClient --headless --quit-after 30 --input-check
//       scripted clicks on and beside the test button, and a key (godot.runic_client_input)
//   RunicClient --headless --quit-after 600 --walk-check
//       scripted keys: R (walk on its own), Space (jump), R again (godot.runic_client_walk)
//   RunicClient --headless --quit-after 300 --walk-check --physics-check
//       the same walk, and each frame the capsule's collision lines (the physics geometry layer,
//       slice 8) are measured against its gameplay shape (godot.runic_client_physics_view)
//
// Development builds run Godot's editor library on the project folder, as Godot's play button does;
// every argument reaches Godot unchanged, with --path added when it is missing.

#include "Application.hpp"
#include "DebugDemo.hpp"
#include "DebugShapes.hpp"
#include "EntryPoint.hpp"
#include "GodotLayer.hpp"
#include "InputLayer.hpp"
#include "Log.hpp"
#include "MovingEntities.hpp"
#include "PhysicsWorld.hpp"
#include "PresentationLayer.hpp"
#include "RenderData/RenderFrameExchange.hpp"
#include "SimulationLayer.hpp"
#include "SimulationRuntime.hpp"
#include "WalkScene.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <string_view>
#include <utility>
#include <vector>

#ifndef RUNIC_CLIENT_PROJECT_DIR
#define RUNIC_CLIENT_PROJECT_DIR "."
#endif

namespace {

using Runic::Application::AppEvent;
using Runic::Application::Application;
using Runic::Application::ApplicationSpec;
using Runic::Application::CommandLine;
using Runic::Application::kCleanExit;
using Runic::Application::KeyEvent;
using Runic::Application::Layer;
using Runic::Application::LoopPolicy;
using Runic::Application::MouseButtonEvent;

constexpr const char *kProgramName = "RunicClient";
constexpr const char *kInputTraceLayerName = "engine:input_trace";
// RunicClient's own flags: the scene, and the scripted input below.
// Every argument also reaches Godot, which takes a bare word for a scene to open: flags only.
constexpr std::string_view kMovingSceneFlag = "--moving-scene";
constexpr std::string_view kInputCheckFlag = "--input-check";
constexpr std::string_view kWalkCheckFlag = "--walk-check";
constexpr std::string_view kPhysicsCheckFlag = "--physics-check";

// kInputCheckFlag's input, by Godot frame. The test button in Project/Main.tscn spans (20, 20) to
// (220, 80): a click at its center stays in Godot's UI, one beside it reaches the stack, and so
// does the key A.
constexpr std::uint64_t kClickOnButtonFrame = 5;
constexpr float kButtonCenterX = 120.0f;
constexpr float kButtonCenterY = 50.0f;
constexpr std::uint64_t kClickBesideButtonFrame = 8;
constexpr float kBesideButtonX = 600.0f;
constexpr float kBesideButtonY = 400.0f;
constexpr std::uint64_t kKeyFrame = 11;
constexpr std::uint32_t kKeyA = 65; // Godot's KEY_A

// kWalkCheckFlag's keys, by Godot frame: R sets the capsule walking, Space jumps, R stops it.
constexpr std::uint64_t kWalkOnFrame = 30;
constexpr std::uint64_t kJumpFrame = 240;
constexpr std::uint64_t kWalkOffFrame = 480;
constexpr std::uint32_t kKeyR = 'R';
constexpr std::uint32_t kKeySpace = ' ';

// Below the GodotLayer: prints the key presses and mouse-button presses that reach the stack. It
// handles nothing.
class InputTraceLayer final : public Layer {
  public:
    InputTraceLayer() : Layer(kInputTraceLayerName) {}

    void OnEvent(AppEvent &event) override {
        if (const auto *key = event.As<KeyEvent>(); key != nullptr && key->pressed) {
            RUNIC_LOG_INFO("RUNIC: AppEvent {} reached the stack (key {})", KeyEvent::kName,
                           key->key);
        } else if (const auto *button = event.As<MouseButtonEvent>();
                   button != nullptr && button->pressed) {
            RUNIC_LOG_INFO("RUNIC: AppEvent {} reached the stack at ({:.0f}, {:.0f})",
                           MouseButtonEvent::kName, button->x, button->y);
        }
    }
};

// Above every layer, after the GodotLayer applied the frame: the capsule's collision lines (green,
// in the physics geometry layer) against the capsule's gameplay shape, as Godot holds both. Logs
// the worst distance between their centers when the client stops.
class PhysicsCheckLayer final : public Layer {
  public:
    explicit PhysicsCheckLayer(const Runic::GodotBridge::GodotLayer &godot)
        : Layer(kPhysicsCheckLayerName), m_Godot(godot) {}

    void OnUpdate(const Runic::Application::FrameTime &) override {
        Runic::GodotBridge::PresentedInstance shape;
        if (!m_Godot.ReadBackInstance(
                Runic::Core::RenderInstanceId(Runic::Programs::kCapsuleRenderId), shape) ||
            !m_Godot.ReadBackPhysicsSurface(Runic::RenderData::DebugBatch::WorldDepth,
                                            Runic::RenderData::DebugPrimitive::Lines, m_Positions,
                                            m_Colors)) {
            return;
        }
        Runic::Core::Vec3 low(std::numeric_limits<float>::max());
        Runic::Core::Vec3 high(std::numeric_limits<float>::lowest());
        bool found = false;
        for (std::size_t i = 0; i < m_Positions.size(); ++i) {
            if (m_Colors[i] == Runic::Physics::kCharacterColor) {
                low = glm::min(low, m_Positions[i]);
                high = glm::max(high, m_Positions[i]);
                found = true;
            }
        }
        if (!found) {
            return;
        }
        // The shape's center against the middle of the collision capsule, which Jolt lifts by its
        // padding.
        const Runic::Core::Vec3 middle = (low + high) / kHalves;
        const Runic::Core::Vec3 lifted = shape.position + Runic::Core::Vec3(0.0f, kPaddingM, 0.0f);
        m_Worst = std::max(m_Worst, glm::length(middle - lifted));
        ++m_Frames;
    }

    void OnDetach() override {
        RUNIC_LOG_INFO("RUNIC: physics check: the capsule's collision lines up with its shape "
                       "within {:.1f} mm over {} frames",
                       m_Worst * kMillimetresPerMetre, m_Frames);
    }

  private:
    static constexpr const char *kPhysicsCheckLayerName = "engine:physics_check";
    static constexpr float kHalves = 2.0f;
    static constexpr float kPaddingM = 0.02f; // Jolt's character padding
    static constexpr float kMillimetresPerMetre = 1000.0f;

    const Runic::GodotBridge::GodotLayer &m_Godot;
    std::vector<Runic::Core::Vec3> m_Positions;
    std::vector<Runic::RenderData::PackedColor> m_Colors;
    float m_Worst = 0.0f;
    std::uint64_t m_Frames = 0;
};

class RunicClientApplication final : public Application {
  public:
    using Application::Application;

    void OnStopped(int exitCode, std::uint64_t frames) override {
        if (exitCode == kCleanExit) {
            RUNIC_LOG_INFO("RUNIC: {} exited cleanly after {} frames", Spec().name, frames);
        }
    }
    // The frames the Presentation layer writes and the GodotLayer shows; they outlive both layers.
    Runic::RenderData::RenderFrameExchange &Frames() noexcept { return m_Frames; }

  private:
    Runic::RenderData::RenderFrameExchange m_Frames;
};

} // namespace

std::unique_ptr<Application> Runic::Application::CreateApplication(const CommandLine &command) {
    GodotBridge::GodotLayerSpec spec;
    // The template library runs only an exported .pck (COOK-4).
    spec.target = GodotBridge::GodotTarget::Editor;
    spec.arguments = command.Arguments();
    if (!command.Has(GodotBridge::kPathFlag)) {
        GodotBridge::InsertProjectPath(spec.arguments, RUNIC_CLIENT_PROJECT_DIR);
    }
    if (command.Has(kInputCheckFlag)) {
        spec.scriptedClicks = {{kClickOnButtonFrame, kButtonCenterX, kButtonCenterY},
                               {kClickBesideButtonFrame, kBesideButtonX, kBesideButtonY}};
        spec.scriptedKeys = {{kKeyFrame, kKeyA}};
    }
    if (command.Has(kWalkCheckFlag)) {
        spec.scriptedKeys = {
            {kWalkOnFrame, kKeyR}, {kJumpFrame, kKeySpace}, {kWalkOffFrame, kKeyR}};
    }
    const bool movingScene = command.Has(kMovingSceneFlag);

    // Godot paces the frames and reads the frame limit (kQuitAfterFlag) itself.
    auto application = std::make_unique<RunicClientApplication>(ApplicationSpec{
        kProgramName, LoopPolicy::PacedByTopLayer(), ApplicationSpec::kRunUntilStopped});
    spec.frames = &application->Frames();
    spec.developmentOverlay = true;
    // A local simulation: the client world the Presentation layer draws.
    auto simulation = std::make_unique<Simulation::SimulationLayer>(
        std::make_unique<Simulation::SimulationRuntime>());
    Simulation::SimulationRuntime &runtime = simulation->Runtime();
    auto presentation = std::make_unique<Client::PresentationLayer>(
        Client::PresentationLayerSpec{&application->Frames(), &runtime.Entities(), nullptr});
    auto input = std::make_unique<Client::InputLayer>();
    // The scene registers its components and systems on the runtime, before the extractor makes
    // its query.
    std::unique_ptr<Layer> scene;
    if (movingScene) {
        scene = std::make_unique<Programs::MovingEntitiesLayer>(runtime, presentation->Camera());
    } else {
        auto walk =
            std::make_unique<Programs::WalkSceneLayer>(runtime, presentation->Camera(), *input);
        walk->RegisterExtractors(presentation->Extractors(), application->Frames().Meshes());
        scene = std::move(walk);
    }
    presentation->Extractors().Emplace<Client::DebugShapeExtractor>(
        Client::kDebugShapeExtractorName, runtime.Entities());
    if (movingScene) {
        presentation->Extractors().Emplace<Programs::DebugDemoExtractor>(
            Programs::kDebugDemoExtractorName);
    }

    application->PushLayer(std::make_unique<InputTraceLayer>());
    application->PushLayer(std::move(simulation));
    application->PushLayer(std::move(input));
    application->PushLayer(std::move(scene));
    application->PushLayer(std::move(presentation));
    auto godot = std::make_unique<GodotBridge::GodotLayer>(std::move(spec));
    if (command.Has(kPhysicsCheckFlag)) {
        application->PushOverlay(std::make_unique<PhysicsCheckLayer>(*godot));
    }
    application->PushLayer(std::move(godot)); // the top layer
    return application;
}

// The Engine owns main(): it is defined here, once, and hands over to the Engine.
RUNIC_ENTRY_POINT()
