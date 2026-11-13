#include "Spike/RunicSpikePlugin.hpp"

#include "GodotToolsModule.hpp"

#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace Runic::GodotTools {

namespace {
constexpr const char* kToolMenuItem = "Runic: About this editor";
}

void RunicSpikePlugin::_enter_tree() {
    using namespace godot;
    const Dictionary version = Engine::get_singleton()->get_version_info();
    const bool gdscript = ClassDB::class_exists("GDScript");
    UtilityFunctions::print(kPluginLoadedLine, " (GDT-1 spike): the RunicEditor runs the Godot editor through LibGodot, Godot ",
                            version["string"]);
    UtilityFunctions::print("RUNIC: GDScript in this editor build: ", gdscript ? "yes" : "no");

    // Children of the plugin node are freed with it, so the editor's shutdown frees the dialog.
    m_About = memnew(AcceptDialog);
    m_About->set_title("Runic");
    m_About->set_text(String("RunicEditor — slice 0 spike (GDT-1)\n\n") +
                      "The Godot editor running as a library (LibGodot) inside a Runic executable,\n" +
                      "with this test plugin compiled into it and registered at the editor level.\n\n" +
                      "Godot " + String(version["string"]) + (gdscript ? ", GDScript on" : ", GDScript off") + ".");
    add_child(m_About);

    // Project › Tools only: a toolbar control would sit at the far right of the editor's top bar and break its
    // layout. The real Runic UI (docks, inspectors) arrives with GDT-2.
    add_tool_menu_item(kToolMenuItem, callable_mp(this, &RunicSpikePlugin::ShowAbout));
}

void RunicSpikePlugin::_exit_tree() {
    remove_tool_menu_item(kToolMenuItem);
    m_About = nullptr;  // a child of this node
    godot::UtilityFunctions::print(kPluginUnloadedLine);
}

godot::String RunicSpikePlugin::_get_plugin_name() const { return "Runic"; }

void RunicSpikePlugin::ShowAbout() {
    if (m_About != nullptr) {
        m_About->popup_centered();
    }
}

} // namespace Runic::GodotTools
