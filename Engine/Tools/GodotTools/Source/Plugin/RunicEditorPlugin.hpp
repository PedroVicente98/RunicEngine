#pragma once

#include <memory>
#include <optional>

#include <godot_cpp/classes/accept_dialog.hpp>
#include <godot_cpp/classes/confirmation_dialog.hpp>
#include <godot_cpp/classes/editor_plugin.hpp>
#include <godot_cpp/classes/line_edit.hpp>

#include "Project/GameLibrary.hpp"
#include "RunicProject.hpp"

namespace Runic::GodotTools::Detail {

class RunicWorldDock;

// The Runic editor plugin (GDT-2), always on: it opens the project's manifest, offers New for an
// empty Godot project (open decision 2: a plugin, not a patch of the project manager), watches the
// project's editor library, writes the IDE files and shows the world dock. Its menus are in
// Project › Tools only; it adds nothing to the editor's toolbar. Private to Runic::GodotTools.
class RunicEditorPlugin : public godot::EditorPlugin {
    GDCLASS(RunicEditorPlugin, godot::EditorPlugin)

  public:
    void _enter_tree() override;
    void _exit_tree() override;
    void _process(double delta) override;
    godot::String _get_plugin_name() const override;

  protected:
    static void _bind_methods() {}

  private:
    void OpenProject();
    void AskForNewProject();
    void MakeNewProject();
    void GenerateIdeFiles();
    void ReloadGameLibrary();
    void Tell(const godot::String &text);

    std::optional<RunicProject> m_Project;
    std::unique_ptr<GameLibrary> m_Library;
    RunicWorldDock *m_Dock = nullptr;         // a child of the editor's dock, freed by Godot
    godot::AcceptDialog *m_Message = nullptr; // children of this node
    godot::ConfirmationDialog *m_NewProject = nullptr;
    godot::LineEdit *m_GameName = nullptr;
    double m_SincePoll = 0.0;
};

} // namespace Runic::GodotTools::Detail
