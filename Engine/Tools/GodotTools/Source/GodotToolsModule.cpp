// RegisterEditorTools (GodotToolsModule.hpp): the editor-level classes and the context they read.

#include "GodotToolsModule.hpp"

#include <godot_cpp/classes/editor_plugin_registration.hpp>
#include <godot_cpp/core/class_db.hpp>

#include "Docks/RunicPhysicsDock.hpp"
#include "Docks/RunicWorldDock.hpp"
#include "GodotRuntime.hpp"
#include "Plugin/EditorContext.hpp"
#include "Plugin/RunicEditorPlugin.hpp"

namespace Runic::GodotTools {

namespace Detail {

namespace {
EditorToolsSpec &Stored() {
    static EditorToolsSpec spec;
    return spec;
}
} // namespace

const EditorToolsSpec &Context() noexcept { return Stored(); }
void SetContext(const EditorToolsSpec &spec) { Stored() = spec; }

} // namespace Detail

using GodotBridge::GodotInitLevel;

void RegisterEditorTools(GodotBridge::GodotRuntime &runtime, const EditorToolsSpec &spec) {
    Detail::SetContext(spec);
    // Editor plugins register only at the editor level, which the client never reaches.
    runtime.AddInitializeHook([](GodotInitLevel level) {
        if (level == GodotInitLevel::Editor) {
            godot::ClassDB::register_internal_class<Detail::RunicWorldDock>();
            godot::ClassDB::register_internal_class<Detail::RunicPhysicsDock>();
            godot::ClassDB::register_internal_class<Detail::RunicEditorPlugin>();
            godot::EditorPlugins::add_by_type<Detail::RunicEditorPlugin>(); // always on
        }
    });
    runtime.AddDeinitializeHook([](GodotInitLevel level) {
        if (level == GodotInitLevel::Editor) {
            godot::EditorPlugins::remove_by_type<Detail::RunicEditorPlugin>();
        }
    });
}

} // namespace Runic::GodotTools
