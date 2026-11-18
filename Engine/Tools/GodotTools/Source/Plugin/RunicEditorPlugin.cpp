// The Runic editor plugin (RunicEditorPlugin.hpp).

#include "Plugin/RunicEditorPlugin.hpp"

#include <format>
#include <string>

#include <godot_cpp/classes/editor_file_system.hpp>
#include <godot_cpp/classes/editor_interface.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/input_event_mouse_button.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/script_editor.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>

#include "Docks/RunicPhysicsDock.hpp"
#include "Docks/RunicWorldDock.hpp"
#include "EditorCook.hpp"
#include "IdeBridge.hpp"
#include "Log.hpp"
#include "Plugin/EditorContext.hpp"

namespace Runic::GodotTools::Detail {

namespace {

constexpr const char *kPluginName = "Runic";
constexpr const char *kNewProjectItem = "Runic: make this a Runic project";
constexpr const char *kIdeFilesItem = "Runic: generate IDE files";
constexpr const char *kReloadItem = "Runic: reload the game editor library";
constexpr const char *kCookItem = "Runic: cook the project";
constexpr const char *kNewProjectTitle = "Make this a Runic project";
constexpr const char *kMessageTitle = "Runic";
constexpr const char *kProjectRoot = "res://";
constexpr const char *kProjectNameSetting = "application/config/name";
constexpr const char *kGDScriptClass = "GDScript";
constexpr const char *kConfirmedSignal = "confirmed";
// The editor library is looked at twice a second: a rebuilt library reloads within a second.
constexpr double kPollSeconds = 0.5;
// --runic-cook waits this long after the editor's last scan and import, then cooks; and quits this
// long after a help page made Godot's help thread finish (Godot 4.7.2 crashes on a quit before
// that thread queued its last call: see ARCHITECTURE.md).
constexpr double kImportSettleSeconds = 1.0;
constexpr double kQuitAfterHelpSeconds = 1.0;
constexpr const char *kAnyHelpTopic = "class_name:Object";
constexpr const char *kResPrefix = "res://";

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
    add_tool_menu_item(kCookItem, callable_mp(this, &RunicEditorPlugin::CookProject));

    // The Cooker integration: collision presets at import, a cook before each export, the
    // collider's gizmo.
    m_Import.instantiate();
    add_scene_post_import_plugin(m_Import);
    m_Export.instantiate();
    m_Export->SetCook(callable_mp(this, &RunicEditorPlugin::CookProject));
    add_export_plugin(m_Export);
    m_ColliderGizmo.instantiate();
    m_ColliderGizmo->Prepare();
    add_node_3d_gizmo_plugin(m_ColliderGizmo);

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
    remove_tool_menu_item(kCookItem);
    remove_scene_post_import_plugin(m_Import);
    remove_export_plugin(m_Export);
    remove_node_3d_gizmo_plugin(m_ColliderGizmo);
    m_Import.unref();
    m_Export.unref();
    m_ColliderGizmo.unref();
    m_Library.reset();
    m_Message = nullptr; // children of this node
    m_NewProject = nullptr;
    m_GameName = nullptr;
    RUNIC_LOG_INFO("{}", kPluginUnloadedLine);
}

void RunicEditorPlugin::_process(double delta) {
    if (Context().cookAndQuit) {
        CookAndQuitWhenImported(delta);
    }
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

bool RunicEditorPlugin::Cook(const std::vector<std::string> &scenes) {
    if (!m_Project.has_value()) {
        RUNIC_LOG_ERROR("{}: this is not a Runic project", kCookFailedLine);
        return false;
    }
    const ProjectManifest &manifest = m_Project->manifest;
    EditorCookSpec spec;
    spec.scenes = scenes;
    if (spec.scenes.empty()) {
        spec.scenes.push_back(std::string(kResPrefix) + manifest.cook.world);
    }
    spec.output = m_Project->root / manifest.paths.cooked;
    spec.contentFolder = manifest.paths.content;
    spec.origin = Context().editedSceneOrigin;
    auto report = CookEditorScenes(spec);
    if (!report) {
        RUNIC_LOG_ERROR("{}: {}", kCookFailedLine, report.GetError().message);
        return false;
    }
    const Cooker::CookReport &cooked = report.Value();
    if (!cooked.succeeded) {
        RUNIC_LOG_ERROR("{}: {} problems", kCookFailedLine, cooked.problems.size());
        for (const std::string &problem : cooked.problems) {
            RUNIC_LOG_ERROR("RUNIC: cook problem: {}", problem);
        }
        return false;
    }
    RUNIC_LOG_INFO("{} in {:.0f} ms: {} meshes, {} collisions, {} cells, {} placements, {} "
                   "colliders, {} collision triangles ({} welded vertices, {} dropped triangles) "
                   "into {}",
                   kCookSucceededLine, cooked.seconds * 1000.0, cooked.meshes, cooked.collisions,
                   cooked.cells, cooked.placements, cooked.colliders, cooked.collisionTrianglesOut,
                   cooked.weldedVertices, cooked.droppedTriangles, spec.output.string());
    if (Simulation::SimulationRuntime *world = Context().localWorld) {
        auto loaded = LoadCookedCells(*world, spec.output);
        if (!loaded) {
            RUNIC_LOG_WARNING("RUNIC: the local world did not load the cook: {}",
                              loaded.GetError().message);
        } else {
            RUNIC_LOG_INFO("RUNIC: the local world loaded the cook: {} cells, {} bodies ({} "
                           "replaced)",
                           loaded.Value().cells, loaded.Value().bodies, loaded.Value().removed);
        }
    }
    return true;
}

void RunicEditorPlugin::CookProject() {
    // The world scene; until the project has one (region tools, slice 21), the scene being edited.
    std::vector<std::string> scenes;
    if (m_Project.has_value()) {
        const std::string world = std::string(kResPrefix) + m_Project->manifest.cook.world;
        godot::Node *edited = godot::EditorInterface::get_singleton()->get_edited_scene_root();
        if (!godot::ResourceLoader::get_singleton()->exists(ToGodot(world)) && edited != nullptr &&
            !edited->get_scene_file_path().is_empty()) {
            scenes.push_back(ToStd(edited->get_scene_file_path()));
            RUNIC_LOG_INFO("RUNIC: the project has no {} yet: the cook takes the edited scene, {}",
                           world, scenes.back());
        }
    }
    const bool cooked = Cook(scenes);
    Tell(cooked ? "The project is cooked: see the log for what it holds."
                : "The cook failed: the log lists its problems.");
}

void RunicEditorPlugin::CookAndQuitWhenImported(double delta) {
    if (m_CookedOnStart) {
        m_QuitIn -= delta;
        if (m_QuitIn <= 0.0) {
            if (Context().cookFinished) {
                Context().cookFinished(m_ExitCode);
            }
            get_tree()->quit(m_ExitCode);
        }
        return;
    }
    godot::EditorFileSystem *files =
        godot::EditorInterface::get_singleton()->get_resource_filesystem();
    if (files->is_scanning() || files->is_importing()) {
        m_ImportedFor = 0.0;
        return;
    }
    m_ImportedFor += delta;
    if (m_ImportedFor < kImportSettleSeconds) {
        return;
    }
    m_CookedOnStart = true;
    m_ExitCode = Cook(Context().cookScenes) ? kCookSucceededExitCode : kCookFailedExitCode;
    if (godot::ScriptEditor *scripts =
            godot::EditorInterface::get_singleton()->get_script_editor()) {
        scripts->goto_help(kAnyHelpTopic);
    }
    m_QuitIn = kQuitAfterHelpSeconds;
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
