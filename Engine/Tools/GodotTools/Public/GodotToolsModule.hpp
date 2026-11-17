#pragma once

#include <filesystem>
#include <string>

#include "IdeBridge.hpp"

namespace Runic::GodotBridge {
class GodotRuntime;
}
namespace Runic::Simulation {
class SimulationRuntime;
}

namespace Runic::GodotTools {

// What the RunicEditor hands its tools.
struct EditorToolsSpec {
    // The editor's local simulation, which the world dock inspects; nullptr shows no dock. It
    // outlives the editor's Godot instance.
    Simulation::SimulationRuntime *localWorld = nullptr;
    // The runic-mmo template New copies, and the Engine version a new manifest names.
    std::filesystem::path templateDirectory;
    std::string engineVersion;
    // What Generate IDE files writes for.
    IdeFilesSpec ide;
};

// Registers the RunicEditor's tool classes with Godot through the runtime's initialization hooks,
// before the instance is created: the Runic editor plugin (always on, no plugin.cfg) and its dock.
void RegisterEditorTools(GodotBridge::GodotRuntime &runtime, const EditorToolsSpec &spec = {});

// The lines the plugin prints when it loads and unloads; the editor's CTests look for them.
inline constexpr const char *kPluginLoadedLine = "RUNIC: Runic editor plugin loaded";
inline constexpr const char *kPluginUnloadedLine = "RUNIC: Runic editor plugin unloaded";

} // namespace Runic::GodotTools
