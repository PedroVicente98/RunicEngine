#pragma once

#include <filesystem>
#include <functional>
#include <string>
#include <vector>

#include "IdeBridge.hpp"
#include "WorldSpace.hpp"

namespace Runic::GodotBridge {
class GodotRuntime;
}
namespace Runic::Simulation {
class SimulationRuntime;
}

namespace Runic::GodotTools {

// What the RunicEditor hands its tools.
struct EditorToolsSpec {
    // The editor's local simulation, which the world and physics docks inspect; nullptr shows
    // neither dock. It outlives the editor's Godot instance.
    Simulation::SimulationRuntime *localWorld = nullptr;
    // The world position the edited scene's origin stands for: where the physics dock's "see what
    // the server sees" layer puts the local simulation's collision in the editor's viewports. The
    // world's origin until region tools (GDT-8) open a region at its own origin.
    Core::WorldPosition editedSceneOrigin;
    // The runic-mmo template New copies, and the Engine version a new manifest names.
    std::filesystem::path templateDirectory;
    std::string engineVersion;
    // What Generate IDE files writes for.
    IdeFilesSpec ide;

    // --runic-cook (GDT-4): once the editor has imported the project, the plugin cooks these scenes
    // (none: the manifest's world scene) into the manifest's cooked folder, loads the result into
    // the local world, prints the report and quits, with exit code 0 when the cook succeeded. How
    // build machines and CTests cook: the RunicEditor without a window.
    bool cookAndQuit = false;
    std::vector<std::string> cookScenes;
    // Called with --runic-cook's exit code just before Godot quits, so the program stops with it
    // (Godot's own quit code does not reach the application).
    std::function<void(int exitCode)> cookFinished;
};

// Registers the RunicEditor's tool classes with Godot through the runtime's initialization hooks,
// before the instance is created: the Runic editor plugin (always on, no plugin.cfg) and its docks.
void RegisterEditorTools(GodotBridge::GodotRuntime &runtime, const EditorToolsSpec &spec = {});

// The lines the plugin prints when it loads and unloads; the editor's CTests look for them.
inline constexpr const char *kPluginLoadedLine = "RUNIC: Runic editor plugin loaded";
inline constexpr const char *kPluginUnloadedLine = "RUNIC: Runic editor plugin unloaded";
// The cook's report lines (GDT-4).
inline constexpr const char *kCookSucceededLine = "RUNIC: cook succeeded";
inline constexpr const char *kCookFailedLine = "RUNIC: cook failed";
// The exit codes of --runic-cook.
inline constexpr int kCookSucceededExitCode = 0;
inline constexpr int kCookFailedExitCode = 1;

} // namespace Runic::GodotTools
