#include "GodotToolsModule.hpp"

#include "GodotRuntime.hpp"
#include "Spike/RunicSpikePlugin.hpp"

#include <godot_cpp/classes/editor_plugin_registration.hpp>
#include <godot_cpp/core/class_db.hpp>

namespace Runic::GodotTools {

using GodotBridge::GodotInitLevel;

void RegisterEditorTools(GodotBridge::GodotRuntime& runtime) {
    // Editor plugins register only at the editor level, which the client never reaches.
    runtime.AddInitializeHook([](GodotInitLevel level) {
        if (level == GodotInitLevel::Editor) {
            godot::ClassDB::register_internal_class<RunicSpikePlugin>();
            godot::EditorPlugins::add_by_type<RunicSpikePlugin>();  // always on: no plugin.cfg, no toggle
        }
    });
    runtime.AddDeinitializeHook([](GodotInitLevel level) {
        if (level == GodotInitLevel::Editor) {
            godot::EditorPlugins::remove_by_type<RunicSpikePlugin>();
        }
    });
}

} // namespace Runic::GodotTools
