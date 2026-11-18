// The physics inspector dock (RunicPhysicsDock.hpp).

#include "Docks/RunicPhysicsDock.hpp"

#include <cmath>
#include <format>
#include <string>
#include <vector>

#include <godot_cpp/classes/camera3d.hpp>
#include <godot_cpp/classes/editor_interface.hpp>
#include <godot_cpp/classes/sub_viewport.hpp>
#include <godot_cpp/classes/tree_item.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/classes/world3d.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>

#include "Log.hpp"

namespace Runic::GodotTools::Detail {

namespace {

using RenderData::DebugBatch;
using RenderData::DebugPrimitive;

// Four refreshes a second for the lists, as the world dock.
constexpr double kRefreshSeconds = 0.25;
// The layer is drawn around the editor camera's 16 m cell: a camera moving inside it redraws
// nothing.
constexpr double kOriginCellM = 16.0;
// The editor's first 3D viewport: the one whose camera the dock follows.
constexpr int32_t kFirstViewport = 0;
constexpr int kNameColumn = 0;
constexpr int kShapeColumn = 1;
constexpr int kMotionColumn = 2;
constexpr int kValueColumn = 1;
constexpr int kBodyColumns = 3;
constexpr int kPropertyColumns = 2;
constexpr float kListMinimumHeight = 160.0f;
constexpr const char *kBodyTitle = "Body";
constexpr const char *kShapeTitle = "Shape";
constexpr const char *kMotionTitle = "Motion";
constexpr const char *kPropertyTitle = "Property";
constexpr const char *kValueTitle = "Value";
constexpr const char *kServerViewLabel = "See what the server sees";
constexpr const char *kOverVisualsLabel = "Over the visuals";
constexpr const char *kPickLabel = "Pick a body in the viewport";
constexpr const char *kNoPhysicsText = "The local simulation runs no physics";
constexpr const char *kItemSelectedSignal = "item_selected";
constexpr const char *kToggledSignal = "toggled";
constexpr const char *kOverVisualsMode = "over the visuals";
constexpr const char *kDepthTestedMode = "depth-tested";
// The check methods' names and arguments, as tests call them.
constexpr const char *kShownBodiesMethod = "shown_bodies";
constexpr const char *kSelectBodyMethod = "select_body";
constexpr const char *kSelectedPropertyMethod = "selected_property";
constexpr const char *kPickRayMethod = "pick_ray";
constexpr const char *kServerViewSurfaceMethod = "server_view_surface";
constexpr const char *kServerViewOverVisualsMethod = "server_view_drawn_over_visuals";
constexpr const char *kServerViewShownMethod = "server_view_shown";
constexpr const char *kIndexArgument = "index";
constexpr const char *kNameArgument = "name";
constexpr const char *kOriginArgument = "origin";
constexpr const char *kDirectionArgument = "direction";
constexpr const char *kBatchArgument = "batch";
constexpr const char *kPrimitiveArgument = "primitive";
constexpr int64_t kBatchCount = 2;
constexpr int64_t kPrimitiveCount = 2;

godot::String ToGodot(std::string_view text) {
    return godot::String::utf8(text.data(), static_cast<int64_t>(text.size()));
}

double SnapToCell(double metres) { return std::floor(metres / kOriginCellM) * kOriginCellM; }

godot::Camera3D *EditorCamera() {
    godot::EditorInterface *editor = godot::EditorInterface::get_singleton();
    godot::SubViewport *viewport =
        editor != nullptr ? editor->get_editor_viewport_3d(kFirstViewport) : nullptr;
    return viewport != nullptr ? viewport->get_camera_3d() : nullptr;
}

} // namespace

void RunicPhysicsDock::_bind_methods() {
    godot::ClassDB::bind_method(godot::D_METHOD(kShownBodiesMethod),
                                &RunicPhysicsDock::ShownBodies);
    godot::ClassDB::bind_method(godot::D_METHOD(kSelectBodyMethod, kIndexArgument),
                                &RunicPhysicsDock::SelectBody);
    godot::ClassDB::bind_method(godot::D_METHOD(kSelectedPropertyMethod, kNameArgument),
                                &RunicPhysicsDock::SelectedProperty);
    godot::ClassDB::bind_method(
        godot::D_METHOD(kPickRayMethod, kOriginArgument, kDirectionArgument),
        &RunicPhysicsDock::PickRay);
    godot::ClassDB::bind_method(
        godot::D_METHOD(kServerViewSurfaceMethod, kBatchArgument, kPrimitiveArgument),
        &RunicPhysicsDock::ServerViewSurface);
    godot::ClassDB::bind_method(godot::D_METHOD(kServerViewOverVisualsMethod),
                                &RunicPhysicsDock::ServerViewDrawnOverVisuals);
    godot::ClassDB::bind_method(godot::D_METHOD(kServerViewShownMethod),
                                &RunicPhysicsDock::ServerViewShown);
}

void RunicPhysicsDock::Attach(Simulation::SimulationRuntime &runtime,
                              Core::WorldPosition sceneOrigin) {
    m_Runtime = &runtime;
    m_SceneOrigin = sceneOrigin;
    m_Inspector = std::make_unique<PhysicsInspector>(runtime);
}

void RunicPhysicsDock::_ready() {
    set_name(kTitle);
    m_Summary = memnew(godot::Label);
    m_Summary->set_autowrap_mode(godot::TextServer::AUTOWRAP_WORD_SMART);
    add_child(m_Summary);

    m_ServerView = memnew(godot::CheckBox);
    m_ServerView->set_name(kServerViewNode);
    m_ServerView->set_text(kServerViewLabel);
    m_ServerView->set_pressed(m_ServerViewOn);
    m_ServerView->connect(kToggledSignal,
                          callable_mp(this, &RunicPhysicsDock::OnServerViewToggled));
    add_child(m_ServerView);

    m_OverVisuals = memnew(godot::CheckBox);
    m_OverVisuals->set_name(kOverVisualsNode);
    m_OverVisuals->set_text(kOverVisualsLabel);
    m_OverVisuals->set_pressed(m_Settings.overVisuals);
    m_OverVisuals->connect(kToggledSignal,
                           callable_mp(this, &RunicPhysicsDock::OnOverVisualsToggled));
    add_child(m_OverVisuals);

    m_Pick = memnew(godot::Button);
    m_Pick->set_name(kPickNode);
    m_Pick->set_text(kPickLabel);
    m_Pick->set_toggle_mode(true);
    m_Pick->connect(kToggledSignal, callable_mp(this, &RunicPhysicsDock::OnPickToggled));
    add_child(m_Pick);

    m_Bodies = memnew(godot::Tree);
    m_Bodies->set_name(kBodiesNode);
    m_Bodies->set_columns(kBodyColumns);
    m_Bodies->set_column_titles_visible(true);
    m_Bodies->set_column_title(kNameColumn, kBodyTitle);
    m_Bodies->set_column_title(kShapeColumn, kShapeTitle);
    m_Bodies->set_column_title(kMotionColumn, kMotionTitle);
    m_Bodies->set_hide_root(true);
    m_Bodies->set_custom_minimum_size(godot::Vector2(0.0f, kListMinimumHeight));
    m_Bodies->set_v_size_flags(SIZE_EXPAND_FILL);
    m_Bodies->connect(kItemSelectedSignal, callable_mp(this, &RunicPhysicsDock::OnBodySelected));
    add_child(m_Bodies);

    m_Properties = memnew(godot::Tree);
    m_Properties->set_name(kPropertiesNode);
    m_Properties->set_columns(kPropertyColumns);
    m_Properties->set_column_titles_visible(true);
    m_Properties->set_column_title(kNameColumn, kPropertyTitle);
    m_Properties->set_column_title(kValueColumn, kValueTitle);
    m_Properties->set_hide_root(true);
    m_Properties->set_custom_minimum_size(godot::Vector2(0.0f, kListMinimumHeight));
    m_Properties->set_v_size_flags(SIZE_EXPAND_FILL);
    add_child(m_Properties);

    set_process(true);
    Refresh();
    RefreshServerView(true);
}

void RunicPhysicsDock::_process(double delta) {
    m_SinceRefresh += delta;
    if (m_SinceRefresh >= kRefreshSeconds) {
        m_SinceRefresh = 0.0;
        Refresh();
    }
    RefreshServerView(false);
}

void RunicPhysicsDock::_exit_tree() {
    if (m_Layer.IsCreated()) {
        const ServerViewDrawn &drawn = m_Layer.Drawn();
        const double meanMicroseconds =
            drawn.refreshes == 0
                ? 0.0
                : std::chrono::duration<double, std::micro>(m_RefreshTime).count() /
                      static_cast<double>(drawn.refreshes);
        RUNIC_LOG_INFO("RUNIC: server view stopped after {} refreshes of {:.0f} us: {} line "
                       "vertices, {} triangle vertices, {} mesh instances of {} cached meshes ({} "
                       "uploads)",
                       drawn.refreshes, meanMicroseconds, drawn.lineVertices,
                       drawn.triangleVertices, drawn.meshInstances, drawn.cachedMeshes,
                       drawn.meshUploads);
    }
    m_Layer.Destroy();
}

void RunicPhysicsDock::Refresh() {
    if (m_Inspector == nullptr || m_Summary == nullptr) {
        return;
    }
    const PhysicsSummary summary = m_Inspector->Summary();
    if (!summary.enabled) {
        m_Summary->set_text(kNoPhysicsText);
        m_ShownBodies = 0;
        if (!m_ReportedDock) {
            m_ReportedDock = true;
            RUNIC_LOG_INFO("RUNIC: physics dock: {}", kNoPhysicsText);
        }
        return;
    }
    std::string text = std::format(
        "Physics · step {}\n{} bodies · {} active · {} shapes · {} characters · {} owned by "
        "entities",
        summary.steps, summary.bodies, summary.activeBodies, summary.shapes, summary.characters,
        summary.entityBodies);
    if (const std::uint32_t dropped = m_Layer.Drawn().droppedVertices; dropped > 0) {
        text += std::format("\nThe layer is full: {} vertices not drawn (fewer bodies near the "
                            "camera draw them all)",
                            dropped);
    }
    m_Summary->set_text(ToGodot(text));

    const Physics::BodyHandle selected = m_Inspector->Selected();
    m_Bodies->clear();
    godot::TreeItem *root = m_Bodies->create_item();
    const auto bodies = m_Inspector->Bodies(ViewOrigin(), m_Settings.radiusM);
    for (const Physics::BodyHandle body : bodies) {
        const InspectedBody inspected = m_Inspector->Inspect(body);
        godot::TreeItem *item = m_Bodies->create_item(root);
        item->set_text(kNameColumn, ToGodot(inspected.label));
        item->set_text(kShapeColumn, ToGodot(inspected.shape));
        item->set_text(kMotionColumn, ToGodot(inspected.motion));
        item->set_metadata(kNameColumn, static_cast<int64_t>(body.Value()));
        if (body == selected) {
            item->select(kNameColumn);
        }
    }
    m_ShownBodies = static_cast<int64_t>(bodies.size());
    ShowSelected();

    // Reported once physics has stepped: before, the bridge's bodies are still requests.
    if (!m_ReportedDock && summary.steps > 0) {
        m_ReportedDock = true;
        RUNIC_LOG_INFO("RUNIC: physics dock shows {} bodies of simulation {} ({} active, {} "
                       "shapes, {} characters)",
                       m_ShownBodies, m_Runtime->Id().Value(), summary.activeBodies, summary.shapes,
                       summary.characters);
    }
}

void RunicPhysicsDock::RefreshServerView(bool force) {
    if (m_Inspector == nullptr) {
        return;
    }
    if (!m_Layer.IsCreated()) {
        godot::Camera3D *camera = EditorCamera();
        godot::Viewport *viewport = camera != nullptr ? camera->get_viewport() : get_viewport();
        const godot::Ref<godot::World3D> world =
            viewport != nullptr ? viewport->find_world_3d() : godot::Ref<godot::World3D>();
        if (world.is_null()) {
            return;
        }
        m_Layer.Create(world->get_scenario());
        m_Layer.SetVisible(m_ServerViewOn);
    }
    if (!m_ServerViewOn) {
        return;
    }
    const std::uint64_t steps = m_Inspector->Summary().steps;
    const Core::WorldPosition origin = ViewOrigin();
    const Physics::BodyHandle selected = m_Inspector->Selected();
    if (!force && !m_ServerViewDirty && steps == m_DrawnSteps && origin == m_DrawnOrigin &&
        selected == m_DrawnSelection) {
        return;
    }
    const auto started = std::chrono::steady_clock::now();
    m_Geometry.Clear();
    m_LastDraw = m_Inspector->DrawServerView(origin, m_Settings, m_Geometry, &m_Meshes);
    const Core::WorldDelta translation = origin - m_SceneOrigin;
    m_Layer.Show(m_Geometry, m_Meshes,
                 godot::Vector3(static_cast<float>(translation.x),
                                static_cast<float>(translation.y),
                                static_cast<float>(translation.z)));
    m_RefreshTime += std::chrono::steady_clock::now() - started;
    m_DrawnSteps = steps;
    m_DrawnOrigin = origin;
    m_DrawnSelection = selected;
    m_ServerViewDirty = false;
    ReportServerView();
}

void RunicPhysicsDock::ReportServerView() {
    if (m_ReportedServerView || !m_Inspector->Summary().enabled) {
        return;
    }
    if (!m_LastDraw.physics.available) {
        m_ReportedServerView = true;
        RUNIC_LOG_INFO("RUNIC: server view: this build has no physics debug view "
                       "(RUNIC_PHYSICS_DEBUG_VIEW is off)");
        return;
    }
    if (m_LastDraw.physics.bodies == 0) {
        return; // nothing to tell before the first step
    }
    m_ReportedServerView = true;
    const ServerViewDrawn &drawn = m_Layer.Drawn();
    RUNIC_LOG_INFO("RUNIC: server view draws {} line vertices, {} triangle vertices and {} mesh "
                   "instances of {} bodies in the editor's viewport ({})",
                   drawn.lineVertices, drawn.triangleVertices, drawn.meshInstances,
                   m_LastDraw.physics.bodies,
                   ServerViewDrawnOverVisuals() ? kOverVisualsMode : kDepthTestedMode);
}

bool RunicPhysicsDock::IsPicking() const noexcept {
    return m_Pick != nullptr && m_Pick->is_pressed();
}

void RunicPhysicsDock::StopPicking() {
    if (m_Pick != nullptr) {
        m_Pick->set_pressed(false);
    }
}

bool RunicPhysicsDock::PickRay(godot::Vector3 origin, godot::Vector3 direction) {
    if (m_Inspector == nullptr) {
        return false;
    }
    const Core::WorldPosition from{m_SceneOrigin.x + origin.x, m_SceneOrigin.y + origin.y,
                                   m_SceneOrigin.z + origin.z};
    const Physics::BodyHandle picked =
        m_Inspector->Pick(from, Core::Vec3(direction.x, direction.y, direction.z));
    m_Inspector->Select(picked);
    m_ServerViewDirty = true;
    Refresh(); // marks it in the list and shows its properties
    return picked.IsValid();
}

bool RunicPhysicsDock::SelectBody(int64_t index) {
    godot::TreeItem *root = m_Bodies != nullptr ? m_Bodies->get_root() : nullptr;
    if (root == nullptr || index < 0 || index >= root->get_child_count()) {
        return false;
    }
    root->get_child(static_cast<int32_t>(index))->select(kNameColumn);
    OnBodySelected(); // the list's own signal does the same; once more changes nothing
    return m_Inspector->Selected().IsValid();
}

godot::String RunicPhysicsDock::SelectedProperty(const godot::String &name) const {
    godot::TreeItem *root = m_Properties != nullptr ? m_Properties->get_root() : nullptr;
    if (root == nullptr) {
        return {};
    }
    for (godot::TreeItem *item = root->get_first_child(); item != nullptr;
         item = item->get_next()) {
        if (item->get_text(kNameColumn) == name) {
            return item->get_text(kValueColumn);
        }
    }
    return {};
}

godot::PackedVector3Array RunicPhysicsDock::ServerViewSurface(int64_t batch,
                                                              int64_t primitive) const {
    godot::PackedVector3Array surface;
    if (batch < 0 || batch >= kBatchCount || primitive < 0 || primitive >= kPrimitiveCount) {
        return surface;
    }
    for (const Core::Vec3 &p : m_Layer.ReadSurface(static_cast<DebugBatch>(batch),
                                                   static_cast<DebugPrimitive>(primitive))) {
        surface.push_back(godot::Vector3(p.x, p.y, p.z));
    }
    return surface;
}

bool RunicPhysicsDock::ServerViewDrawnOverVisuals() const {
    return m_Settings.overVisuals && m_Layer.DrawsOverEverything(DebugBatch::Overlay);
}

void RunicPhysicsDock::OnBodySelected() {
    if (godot::TreeItem *item = m_Bodies->get_selected()) {
        const auto value =
            static_cast<std::uint32_t>(static_cast<int64_t>(item->get_metadata(kNameColumn)));
        const Physics::BodyHandle body(value);
        if (body != m_Inspector->Selected()) {
            m_Inspector->Select(body);
            m_ServerViewDirty = true;
        }
        ShowSelected();
    }
}

void RunicPhysicsDock::ShowSelected() {
    m_Properties->clear();
    const Physics::BodyHandle selected = m_Inspector->Selected();
    if (!selected.IsValid()) {
        return;
    }
    godot::TreeItem *root = m_Properties->create_item();
    const InspectedBody inspected = m_Inspector->Inspect(selected);
    for (const InspectedProperty &property : inspected.properties) {
        godot::TreeItem *item = m_Properties->create_item(root);
        item->set_text(kNameColumn, ToGodot(property.name));
        item->set_text(kValueColumn, ToGodot(property.value));
    }
}

void RunicPhysicsDock::OnServerViewToggled(bool on) {
    m_ServerViewOn = on;
    m_Layer.SetVisible(on);
    m_ServerViewDirty = true;
}

void RunicPhysicsDock::OnOverVisualsToggled(bool on) {
    m_Settings.overVisuals = on;
    m_ServerViewDirty = true;
}

void RunicPhysicsDock::OnPickToggled(bool on) {
    if (on) {
        RUNIC_LOG_INFO("RUNIC: physics dock: the next click in a 3D viewport picks a body");
    }
}

Core::WorldPosition RunicPhysicsDock::ViewOrigin() const {
    godot::Vector3 camera; // the scene's origin when there is no editor camera
    if (const godot::Camera3D *editorCamera = EditorCamera()) {
        camera = editorCamera->get_global_position();
    }
    return {m_SceneOrigin.x + SnapToCell(camera.x), m_SceneOrigin.y + SnapToCell(camera.y),
            m_SceneOrigin.z + SnapToCell(camera.z)};
}

} // namespace Runic::GodotTools::Detail
