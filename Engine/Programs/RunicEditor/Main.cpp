// RunicEditor: the Godot editor run through LibGodot, with the Runic tools compiled in
// (GodotTools), on the Engine's entry point since GDB-1 (slice 1): its stack is one GodotLayer
// (editor library, no input forwarding).
//
//   RunicEditor                                     Godot's project manager (New and Open)
//   RunicEditor --editor --path <project>           opens a project, as the project manager does
//   RunicEditor --headless --editor --path <project> --quit-after 120
//                                                   the GDT-1 spike check in CI: open, load the
//                                                   plugin, exit
//
// Every argument goes to Godot unchanged: the project manager opens a project by relaunching this
// executable with
// --path, so the entry point never consumes or rewrites arguments.

#include "Application.hpp"
#include "EntryPoint.hpp"
#include "GodotLayer.hpp"
#include "GodotToolsModule.hpp"
#include "Log.hpp"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace {

using Runic::Application::Application;
using Runic::Application::ApplicationSpec;
using Runic::Application::LoopPolicy;

bool HasArgument(const std::vector<std::string> &arguments, const char *name) {
    return std::find(arguments.begin(), arguments.end(), name) != arguments.end();
}

// Godot's Play buttons relaunch the editor executable without --editor and with --editor-pid to run
// the project as a game. In a Runic project the game runs through Runic's Run (GDT-9, slice 13),
// never inside Godot; until then this safety net stops the relaunch.
bool IsGodotPlayRequest(const std::vector<std::string> &arguments) {
    return HasArgument(arguments, "--editor-pid") && !HasArgument(arguments, "--editor") &&
           !HasArgument(arguments, "-e");
}

class RunicEditorApplication final : public Application {
  public:
    using Application::Application;

    void OnStopped(int exitCode, std::uint64_t frames) override {
        if (exitCode == 0) {
            RUNIC_LOG_INFO("RUNIC: RunicEditor exited cleanly after {} frames", frames);
        }
    }
};

} // namespace

std::unique_ptr<Application> Runic::Application::CreateApplication(const CommandLine &command) {
    if (IsGodotPlayRequest(command.Arguments())) {
        RUNIC_LOG_WARNING("RUNIC: Godot's Play runs the project inside Godot; a Runic project runs "
                          "through Runic's "
                          "Run instead (not available before slice 13).");
        return nullptr;
    }
    GodotBridge::GodotLayerSpec spec;
    spec.target = GodotBridge::GodotTarget::Editor;
    spec.arguments = command.Arguments();
    spec.forwardInput = false; // the editor's input belongs to the editor
    auto layer = std::make_unique<GodotBridge::GodotLayer>(std::move(spec));
    GodotTools::RegisterEditorTools(layer->Runtime());

    auto application = std::make_unique<RunicEditorApplication>(ApplicationSpec{
        "RunicEditor", LoopPolicy::PacedByTopLayer(), 0}); // the editor paces its own frames
    application->PushLayer(std::move(layer));
    return application;
}
