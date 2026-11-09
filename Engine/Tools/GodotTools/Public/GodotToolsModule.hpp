#pragma once

namespace Runic::GodotBridge {
class GodotRuntime;
}

namespace Runic::GodotTools {

// Registers the RunicEditor's tool classes with Godot through the runtime's initialization hooks, before the
// instance is created. Slice 0 (GDT-1 spike): the test editor plugin, always on, with no plugin.cfg.
void RegisterEditorTools(GodotBridge::GodotRuntime& runtime);

// The lines the test plugin prints; the spike's CTest check looks for them.
inline constexpr const char* kPluginLoadedLine = "RUNIC: test plugin loaded";
inline constexpr const char* kPluginUnloadedLine = "RUNIC: test plugin unloaded";

} // namespace Runic::GodotTools
