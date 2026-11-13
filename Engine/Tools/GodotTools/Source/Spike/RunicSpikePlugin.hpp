#pragma once

#include <godot_cpp/classes/accept_dialog.hpp>
#include <godot_cpp/classes/editor_plugin.hpp>

namespace Runic::GodotTools {

// GDT-1 spike: the test plugin that proves the RunicEditor (the Godot editor run through LibGodot) loads Runic
// tool code compiled into the executable. It reports what the spike must find out (the editor opened, and
// without GDScript), adds a Project › Tools item that opens an About dialog, and cleans up on exit. It adds
// nothing to the editor's toolbar. GDT-2 replaces it with the real Runic editor plugin.
class RunicSpikePlugin : public godot::EditorPlugin {
    GDCLASS(RunicSpikePlugin, godot::EditorPlugin)

public:
    void _enter_tree() override;
    void _exit_tree() override;
    godot::String _get_plugin_name() const override;

protected:
    static void _bind_methods() {}

private:
    void ShowAbout();

    godot::AcceptDialog* m_About = nullptr;
};

} // namespace Runic::GodotTools
