// RunicClient: the Engine's own game-agnostic client (RunicDocs › Repositories, build targets and
// processes › Processes): the Engine's client layers with the GodotLayer on top, on a neutral test
// project. Engine slices show their client results here; it is never shipped.
//
// The stack, bottom to top: the input trace, the Simulation layer (a local simulation), the moving
// entities (slice 5: walkers steered by commands, and the camera's orbit), the Presentation layer
// (the camera and the extractors: the walkers as debug shapes, and slice 2's debug scene) and the
// GodotLayer, which draws only the frames the Presentation layer publishes.
//
//   RunicClient
//       a window with the test project; keys and clicks that reach the stack are printed, and a
//       click on the test button is not (Godot's UI keeps it)
//   RunicClient --headless --quit-after 60
//       Godot starts headless and the client exits cleanly (the CTest godot.runic_client_headless)
//   RunicClient --headless --quit-after 30 --input-check
//       scripted clicks on and beside the test button, and a key (godot.runic_client_input)
//
// Development builds run Godot's editor library on the project folder, as Godot's play button does;
// every argument reaches Godot unchanged, with --path added when it is missing.

#include "Application.hpp"
#include "DebugDemo.hpp"
#include "DebugShapes.hpp"
#include "EntryPoint.hpp"
#include "GodotLayer.hpp"
#include "Log.hpp"
#include "MovingEntities.hpp"
#include "PresentationLayer.hpp"
#include "RenderData/RenderFrameExchange.hpp"
#include "SimulationLayer.hpp"
#include "SimulationRuntime.hpp"

#include <cstdint>
#include <memory>
#include <string_view>
#include <utility>

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
// RunicClient's own flag: the scripted input below.
constexpr std::string_view kInputCheckFlag = "--input-check";

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
    // The moving entities register their components and systems on the runtime, before the
    // extractor makes its query.
    auto moving = std::make_unique<Programs::MovingEntitiesLayer>(runtime, presentation->Camera());
    presentation->Extractors().Emplace<Client::DebugShapeExtractor>(
        Client::kDebugShapeExtractorName, runtime.Entities());
    presentation->Extractors().Emplace<Programs::DebugDemoExtractor>(
        Programs::kDebugDemoExtractorName);

    application->PushLayer(std::make_unique<InputTraceLayer>());
    application->PushLayer(std::move(simulation));
    application->PushLayer(std::move(moving));
    application->PushLayer(std::move(presentation));
    application->PushLayer(
        std::make_unique<GodotBridge::GodotLayer>(std::move(spec))); // the top layer
    return application;
}

// The Engine owns main(): it is defined here, once, and hands over to the Engine.
RUNIC_ENTRY_POINT()
