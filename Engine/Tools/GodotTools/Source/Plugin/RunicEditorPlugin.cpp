// The Runic editor plugin (RunicEditorPlugin.hpp).

#include "Plugin/RunicEditorPlugin.hpp"

#include <format>
#include <string>

#include <godot_cpp/classes/editor_file_system.hpp>
#include <godot_cpp/classes/editor_interface.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/input_event_mouse_button.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>

#include "Docks/RunicPhysicsDock.hpp"
#include "Docks/RunicWorldDock.hpp"
#include "IdeBridge.hpp"
#include "Log.hpp"
#include "Plugin/EditorContext.hpp"

namespace Runic::GodotTools::Detail {

namespace {

constexpr const char *kPluginName = "Runic";
constexpr const char *kNewProjectItem = "Runic: make this a Runic project";
constexpr const char *kIdeFilesItem = "Runic: generate IDE files";
constexpr const char *kReloadItem = "Runic: reload the game editor library";
constexpr const char *kNewProjectTitle = "Make this a Runic project";
constexpr const char *kMessageTitle = "Runic";
constexpr const char *kProjectRoot = "res://";
constexpr const char *kProjectNameSetting = "application/config/name";
constexpr const char *kGDScriptClass = "GDScript";
constexpr const char *kConfirmedSignal = "confirmed";
// The editor library is looked at twice a second: a rebuilt library reloads within a second.
constexpr double kPollSeconds = 0.5;

std::string ToStd(const godot::String &text) { return text.utf8().get_data(); }
godot::String ToGodot(std::string_view text) {
    return godot::String::utf8(text.data(), static_cast<int64_t>(text.size()));
}

std::filesystem::path ProjectFolder() {
    return ToStd(godot::ProjectSettings::get_singleton()->globalize_path(kProjectRoot));
}

} // namespace

void RunicEditorPlugin::_enter_tree() {
    const bool gdscript = godot::ClassDB::class_exists(kGDScriptClass);
    RUNIC_LOG_INFO("{}: the RunicEditor runs the Godot editor through LibGodot, Godot {}",
                   kPluginLoadedLine,
                   ToStd(godot::Engine::get_singleton()->get_version_info()["string"]));
    RUNIC_LOG_INFO("RUNIC: GDScript in this editor build: {}", gdscript ? "yes" : "no");

    // Children of the plugin node are freed with it.
    m_Message = memnew(godot::AcceptDialog);
    m_Message->set_title(kMessageTitle);
    add_child(m_Message);

    OpenProject();
    if (!m_Project.has_value()) {
        m_NewProject = memnew(godot::ConfirmationDialog);
        m_NewProject->set_title(kNewProjectTitle);
        m_GameName = memnew(godot::LineEdit);
        m_GameName->set_text(
            godot::ProjectSettings::get_singleton()->get_setting(kProjectNameSetting));
        m_NewProject->add_child(m_GameName);
        m_NewProject->connect(kConfirmedSignal,
                              callable_mp(this, &RunicEditorPlugin::MakeNewProject));
        add_child(m_NewProject);
        add_tool_menu_item(kNewProjectItem,
                           callable_mp(this, &RunicEditorPlugin::AskForNewProject));
    }
    add_tool_menu_item(kIdeFilesItem, callable_mp(this, &RunicEditorPlugin::GenerateIdeFiles));
    add_tool_menu_item(kReloadItem, callable_mp(this, &RunicEditorPlugin::ReloadGameLibrary));

    m_Library = std::make_unique<GameLibrary>();
    m_Library->Open();

    if (Simulation::SimulationRuntime *world = Context().localWorld) {
        m_Dock = memnew(RunicWorldDock);
        m_Dock->Attach(*world);
        add_control_to_dock(DOCK_SLOT_RIGHT_UL, m_Dock);
        m_PhysicsDock = memnew(RunicPhysicsDock);
        m_PhysicsDock->Attach(*world, Context().editedSceneOrigin);
        add_control_to_dock(DOCK_SLOT_RIGHT_BL, m_PhysicsDock);
        // The physics dock picks with a click in any 3D viewport, whatever node is selected.
        set_input_event_forwarding_always_enabled();
    }
    set_process(true);
}

void RunicEditorPlugin::_exit_tree() {
    if (m_Dock != nullptr) {
        remove_control_from_docks(m_Dock);
        m_Dock->queue_free();
        m_Dock = nullptr;
    }
    if (m_PhysicsDock != nullptr) {
        remove_control_from_docks(m_PhysicsDock);
        m_PhysicsDock->queue_free();
        m_PhysicsDock = nullptr;
    }
    if (m_NewProject != nullptr) {
        remove_tool_menu_item(kNewProjectItem);
    }
    remove_tool_menu_item(kIdeFilesItem);
    remove_tool_menu_item(kReloadItem);
    m_Library.reset();
    m_Message = nullptr; // children of this node
    m_NewProject = nullptr;
    m_GameName = nullptr;
    RUNIC_LOG_INFO("{}", kPluginUnloadedLine);
}

void RunicEditorPlugin::_process(double delta) {
    m_SincePoll += delta;
    if (m_SincePoll < kPollSeconds || m_Library == nullptr) {
        return;
    }
    m_SincePoll = 0.0;
    m_Library->Poll();
}

godot::String RunicEditorPlugin::_get_plugin_name() const { return kPluginName; }

int32_t RunicEditorPlugin::_forward_3d_gui_input(godot::Camera3D *camera,
                                                 const godot::Ref<godot::InputEvent> &event) {
    const godot::Ref<godot::InputEventMouseButton> click = event;
    if (m_PhysicsDock == nullptr || !m_PhysicsDock->IsPicking() || camera == nullptr ||
        click.is_null() || !click->is_pressed()) {
        return AFTER_GUI_INPUT_PASS;
    }
    if (click->get_button_index() == godot::MOUSE_BUTTON_LEFT) {
        const godot::Vector2 at = click->get_position();
        m_PhysicsDock->PickRay(camera->project_ray_origin(at), camera->project_ray_normal(at));
    }
    // A left click picks, any other button gives up picking; neither reaches the editor.
    m_PhysicsDock->StopPicking();
    return AFTER_GUI_INPUT_STOP;
}

void RunicEditorPlugin::OpenProject() {
    const std::filesystem::path folder = ProjectFolder();
    if (!IsRunicProject(folder)) {
        RUNIC_LOG_INFO("RUNIC: {} is a Godot project without {}; Project > Tools > {} makes it a "
                       "Runic project",
                       folder.string(), kManifestFileName, kNewProjectItem);
        return;
    }
    auto project = GodotTools::OpenProject(folder);
    if (!project) {
        RUNIC_LOG_ERROR("RUNIC: the project's manifest has problems: {}",
                        project.GetError().message);
        return;
    }
    m_Project = project.Value();
    const ProjectManifest &manifest = m_Project->manifest;
    RUNIC_LOG_INFO("RUNIC: project {} opened (Runic format {}, made with Engine {})",
                   manifest.gameName, manifest.format, manifest.engine);
}

void RunicEditorPlugin::AskForNewProject() {
    if (m_NewProject != nullptr) {
        m_NewProject->popup_centered();
    }
}

void RunicEditorPlugin::MakeNewProject() {
    NewProjectSpec spec;
    spec.templateDirectory = Context().templateDirectory;
    spec.root = ProjectFolder();
    spec.gameName = ToStd(m_GameName->get_text().strip_edges());
    spec.engineVersion = Context().engineVersion;
    auto project = GodotTools::InitializeProject(spec);
    if (!project) {
        Tell(ToGodot(project.GetError().message));
        return;
    }
    m_Project = project.Value();
    remove_tool_menu_item(kNewProjectItem);
    // Godot sees the template's folders and the game's editor library declaration.
    godot::EditorInterface::get_singleton()->get_resource_filesystem()->scan();
    m_Library->Open();
    RUNIC_LOG_INFO("RUNIC: {} is now the Runic project {}", spec.root.string(), spec.gameName);
    Tell(ToGodot(std::format("{} is now a Runic project. Build its editor library ({}) to load "
                             "the game's editor tools.",
                             spec.gameName, kGameEditorExtensionFileName)));
}

void RunicEditorPlugin::GenerateIdeFiles() {
    if (!m_Project.has_value()) {
        Tell(ToGodot(std::format("This is not a Runic project yet: {} first.", kNewProjectItem)));
        return;
    }
    auto report = WriteIdeFiles(*m_Project, Context().ide);
    if (!report) {
        Tell(ToGodot(report.GetError().message));
        return;
    }
    std::string text = "Written:";
    for (const auto &file : report.Value().written) {
        text += "\n  " + file.generic_string();
    }
    for (const std::string &note : report.Value().notes) {
        text += "\n" + note;
    }
    RUNIC_LOG_INFO("RUNIC: IDE files: {}", text);
    Tell(ToGodot(text));
}

void RunicEditorPlugin::ReloadGameLibrary() {
    if (m_Library == nullptr || !m_Library->Reload()) {
        Tell("The game editor library is not loaded: build it first.");
    }
}

void RunicEditorPlugin::Tell(const godot::String &text) {
    m_Message->set_text(text);
    m_Message->popup_centered();
}

} // namespace Runic::GodotTools::Detail
