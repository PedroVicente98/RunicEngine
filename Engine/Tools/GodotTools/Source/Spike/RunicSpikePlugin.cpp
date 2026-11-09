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

    m_Button = memnew(Button);
    m_Button->set_text("Runic");
    m_Button->set_tooltip_text("RunicEditor slice 0: the GDT-1 test plugin");
    m_Button->connect("pressed", callable_mp(this, &RunicSpikePlugin::ShowAbout));
    add_control_to_container(CONTAINER_TOOLBAR, m_Button);

    add_tool_menu_item(kToolMenuItem, callable_mp(this, &RunicSpikePlugin::ShowAbout));
}

void RunicSpikePlugin::_exit_tree() {
    remove_tool_menu_item(kToolMenuItem);
    if (m_Button != nullptr) {
        remove_control_from_container(CONTAINER_TOOLBAR, m_Button);
        memdelete(m_Button);
        m_Button = nullptr;
    }
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
