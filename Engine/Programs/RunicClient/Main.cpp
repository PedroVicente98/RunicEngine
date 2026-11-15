// RunicClient: the Engine's own game-agnostic client (RunicDocs › Repositories, build targets and
// processes › Processes): the Engine's client layers with the GodotLayer on top, on a neutral test
// project. Engine slices show their client results here; it is never shipped.
//
//   RunicClient                               a window with the test project; keys and clicks that
//   reach the stack
//                                             are printed, and a click on the test button is not
//                                             (Godot's UI keeps it)
//   RunicClient --headless --quit-after 60    Godot starts headless and the client exits cleanly
//   (CTest) RunicClient --input-check ...             scripted clicks on and beside the test button
//   and a key (CTest)
//
// Development builds run Godot's editor library on the project folder, as Godot's play button does;
// every argument reaches Godot unchanged, with --path added when it is missing.

#include "Application.hpp"
#include "DebugDemo.hpp"
#include "EntryPoint.hpp"
#include "GodotLayer.hpp"
#include "Log.hpp"
#include "RenderData/RenderFrameExchange.hpp"

#include <memory>
#include <string>
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
using Runic::Application::KeyEvent;
using Runic::Application::Layer;
using Runic::Application::LoopPolicy;
using Runic::Application::MouseButtonEvent;

// Below the GodotLayer: prints the key presses and mouse-button presses that reach the stack. It
// handles nothing.
class InputTraceLayer final : public Layer {
  public:
    InputTraceLayer() : Layer("engine:input_trace") {}

    void OnEvent(AppEvent &event) override {
        if (const auto *key = event.As<KeyEvent>(); key != nullptr && key->pressed) {
            RUNIC_LOG_INFO("RUNIC: AppEvent Key reached the stack (key {})", key->key);
        } else if (const auto *button = event.As<MouseButtonEvent>();
                   button != nullptr && button->pressed) {
            RUNIC_LOG_INFO("RUNIC: AppEvent MouseButton reached the stack at ({:.0f}, {:.0f})",
                           button->x, button->y);
        }
    }
};

class RunicClientApplication final : public Application {
  public:
    using Application::Application;

    void OnStopped(int exitCode, std::uint64_t frames) override {
        if (exitCode == 0) {
            RUNIC_LOG_INFO("RUNIC: RunicClient exited cleanly after {} frames", frames);
        }
    }
    // The frames the debug demo writes and the GodotLayer shows (GDB-2); they outlive both layers.
    Runic::RenderData::RenderFrameExchange &Frames() noexcept { return m_Frames; }

  private:
    Runic::RenderData::RenderFrameExchange m_Frames;
};

} // namespace

std::unique_ptr<Application> Runic::Application::CreateApplication(const CommandLine &command) {
    GodotBridge::GodotLayerSpec spec;
    spec.target = GodotBridge::GodotTarget::Editor; // the template library runs only an exported
                                                    // .pck (COOK-4)
    spec.arguments = command.Arguments();
    if (!command.Has("--path")) {
        spec.arguments.insert(spec.arguments.begin() + 1, {"--path", RUNIC_CLIENT_PROJECT_DIR});
    }
    if (command.Has("--input-check")) {
        // The test button spans (20, 20) to (220, 80): a click at its center stays in Godot's UI,
        // one beside it reaches the stack, and so does the key A (65).
        spec.scriptedClicks = {{5, 120.0f, 50.0f}, {8, 600.0f, 400.0f}};
        spec.scriptedKeys = {{11, 65}};
    }

    auto application = std::make_unique<RunicClientApplication>(ApplicationSpec{
        "RunicClient", LoopPolicy::PacedByTopLayer(), 0}); // Godot reads --quit-after itself
    spec.frames = &application->Frames();
    spec.developmentOverlay = true;
    application->PushLayer(std::make_unique<InputTraceLayer>());
    application->PushLayer(
        std::make_unique<Runic::Programs::DebugDemoLayer>(application->Frames()));
    application->PushLayer(
        std::make_unique<GodotBridge::GodotLayer>(std::move(spec))); // the top layer
    return application;
}

// The Engine owns main(): it is defined here, once, and hands over to the Engine.
RUNIC_ENTRY_POINT()
