#pragma once

#include <memory>
#include <optional>

#include <godot_cpp/classes/accept_dialog.hpp>
#include <godot_cpp/classes/camera3d.hpp>
#include <godot_cpp/classes/confirmation_dialog.hpp>
#include <godot_cpp/classes/editor_plugin.hpp>
#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/classes/line_edit.hpp>

#include "Export/RunicImportPlugin.hpp"
#include "MarkerNodes/RunicCollider3D.hpp"
#include "Project/GameLibrary.hpp"
#include "RunicProject.hpp"

namespace Runic::GodotTools::Detail {

class RunicPhysicsDock;
class RunicWorldDock;

// The Runic editor plugin (GDT-2), always on: it opens the project's manifest, offers New for an
// empty Godot project (open decision 2: a plugin, not a patch of the project manager), watches the
// project's editor library, writes the IDE files and shows the world dock (GDT-2) and the physics
// dock (GDT-3), to which it forwards a click in a 3D viewport while the dock picks a body; it
// registers the Cooker integration (GDT-4: the import plugin's collision presets, the export plugin
// that cooks, RunicCollider3D's gizmo) and cooks from Project › Tools or, with --runic-cook, once
// the project is imported. Its menus are in Project › Tools only; it adds nothing to the editor's
// toolbar. Private to Runic::GodotTools.
class RunicEditorPlugin : public godot::EditorPlugin {
    GDCLASS(RunicEditorPlugin, godot::EditorPlugin)

  public:
    void _enter_tree() override;
    void _exit_tree() override;
    void _process(double delta) override;
    godot::String _get_plugin_name() const override;
    int32_t _forward_3d_gui_input(godot::Camera3D *camera,
                                  const godot::Ref<godot::InputEvent> &event) override;

  protected:
    static void _bind_methods() {}

  private:
    void OpenProject();
    void AskForNewProject();
    void MakeNewProject();
    void GenerateIdeFiles();
    void ReloadGameLibrary();
    // Cooks the scenes (none: the manifest's world scene), loads the result into the local world;
    // returns whether it succeeded.
    bool Cook(const std::vector<std::string> &scenes);
    void CookProject();
    void CookAndQuitWhenImported(double delta);
    void Tell(const godot::String &text);

    std::optional<RunicProject> m_Project;
    std::unique_ptr<GameLibrary> m_Library;
    RunicWorldDock *m_Dock = nullptr; // children of the editor's docks, freed by Godot
    RunicPhysicsDock *m_PhysicsDock = nullptr;
    godot::AcceptDialog *m_Message = nullptr; // children of this node
    godot::ConfirmationDialog *m_NewProject = nullptr;
    godot::LineEdit *m_GameName = nullptr;
    double m_SincePoll = 0.0;
    godot::Ref<RunicImportPlugin> m_Import;
    godot::Ref<RunicExportPlugin> m_Export;
    godot::Ref<RunicColliderGizmo> m_ColliderGizmo;
    // --runic-cook: cooked, and the time left before quitting.
    bool m_CookedOnStart = false;
    int m_ExitCode = 0;
    double m_QuitIn = 0.0;
    double m_ImportedFor = 0.0;
};

} // namespace Runic::GodotTools::Detail
