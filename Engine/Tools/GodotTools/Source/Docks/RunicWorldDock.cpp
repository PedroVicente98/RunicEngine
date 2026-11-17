// The world dock (RunicWorldDock.hpp).

#include "Docks/RunicWorldDock.hpp"

#include <format>
#include <string>

#include <godot_cpp/classes/tree_item.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>

#include "Log.hpp"

namespace Runic::GodotTools::Detail {

namespace {

// Four refreshes a second: lively enough to watch values change, cheap for the editor.
constexpr double kRefreshSeconds = 0.25;
constexpr int kNameColumn = 0;
constexpr int kValueColumn = 1;
constexpr int kTwoColumns = 2;
constexpr const char *kEntitiesTitle = "Entities";
constexpr const char *kComponentsTitle = "Component";
constexpr const char *kValueTitle = "Value";
constexpr const char *kItemSelectedSignal = "item_selected";
constexpr float kListMinimumHeight = 160.0f;
constexpr std::string_view kNoValue = "-";

godot::String ToGodot(std::string_view text) {
    return godot::String::utf8(text.data(), static_cast<int64_t>(text.size()));
}

} // namespace

void RunicWorldDock::Attach(Simulation::SimulationRuntime &runtime) {
    m_Inspector = std::make_unique<WorldInspector>(runtime);
}

void RunicWorldDock::_ready() {
    set_name(kTitle);
    m_Summary = memnew(godot::Label);
    m_Summary->set_autowrap_mode(godot::TextServer::AUTOWRAP_WORD_SMART);
    add_child(m_Summary);

    m_Entities = memnew(godot::Tree);
    m_Entities->set_columns(kTwoColumns);
    m_Entities->set_column_titles_visible(true);
    m_Entities->set_column_title(kNameColumn, kEntitiesTitle);
    m_Entities->set_column_title(kValueColumn, kComponentsTitle);
    m_Entities->set_hide_root(true);
    m_Entities->set_custom_minimum_size(godot::Vector2(0.0f, kListMinimumHeight));
    m_Entities->set_v_size_flags(SIZE_EXPAND_FILL);
    m_Entities->connect(kItemSelectedSignal, callable_mp(this, &RunicWorldDock::OnEntitySelected));
    add_child(m_Entities);

    m_Components = memnew(godot::Tree);
    m_Components->set_columns(kTwoColumns);
    m_Components->set_column_titles_visible(true);
    m_Components->set_column_title(kNameColumn, kComponentsTitle);
    m_Components->set_column_title(kValueColumn, kValueTitle);
    m_Components->set_hide_root(true);
    m_Components->set_custom_minimum_size(godot::Vector2(0.0f, kListMinimumHeight));
    m_Components->set_v_size_flags(SIZE_EXPAND_FILL);
    add_child(m_Components);

    m_Queries = memnew(godot::ItemList);
    m_Queries->set_custom_minimum_size(godot::Vector2(0.0f, kListMinimumHeight));
    add_child(m_Queries);

    set_process(true);
    Refresh();
}

void RunicWorldDock::_process(double delta) {
    m_SinceRefresh += delta;
    if (m_SinceRefresh >= kRefreshSeconds) {
        m_SinceRefresh = 0.0;
        Refresh();
    }
}

void RunicWorldDock::Refresh() {
    if (m_Inspector == nullptr || m_Summary == nullptr) {
        return;
    }
    const WorldSummary summary = m_Inspector->Summary();
    m_Summary->set_text(ToGodot(std::format(
        "Local simulation {} · tick {}\n{} entities · {} component types · {} queries · {} states "
        "· "
        "{} command queues · {} regions",
        summary.simulation.Value(), summary.masterTick, summary.entities, summary.componentTypes,
        summary.queries, summary.states, summary.commandQueues, summary.regions)));

    m_Entities->clear();
    godot::TreeItem *root = m_Entities->create_item();
    const auto entities = m_Inspector->Entities();
    for (const ECS::Entity entity : entities) {
        const InspectedEntity inspected = m_Inspector->Inspect(entity);
        godot::TreeItem *item = m_Entities->create_item(root);
        item->set_text(kNameColumn, ToGodot(inspected.label));
        std::string names;
        for (const InspectedComponent &component : inspected.components) {
            names += names.empty() ? component.name : ", " + component.name;
        }
        item->set_text(kValueColumn, ToGodot(names));
        item->set_metadata(kNameColumn, static_cast<int64_t>(entity.Raw()));
        if (entity == m_Selected) {
            item->select(kNameColumn);
        }
    }
    m_ShownEntities = static_cast<std::uint32_t>(entities.size());
    ShowEntity(m_Selected);

    m_Queries->clear();
    for (const std::string &query : m_Inspector->Queries()) {
        m_Queries->add_item(ToGodot(query));
    }
    for (const std::string &queue : m_Inspector->CommandQueues()) {
        m_Queries->add_item(ToGodot(queue));
    }
    if (!m_Reported) {
        m_Reported = true;
        RUNIC_LOG_INFO("RUNIC: world dock shows {} entities of simulation {} ({} component types, "
                       "{} queries, {} command queues)",
                       m_ShownEntities, summary.simulation.Value(), summary.componentTypes,
                       summary.queries, summary.commandQueues);
    }
}

void RunicWorldDock::OnEntitySelected() {
    if (godot::TreeItem *item = m_Entities->get_selected()) {
        m_Selected = ECS::Entity::FromRaw(
            static_cast<std::uint64_t>(static_cast<int64_t>(item->get_metadata(kNameColumn))));
        ShowEntity(m_Selected);
    }
}

void RunicWorldDock::ShowEntity(ECS::Entity entity) {
    m_Components->clear();
    if (entity.IsNull()) {
        return;
    }
    godot::TreeItem *root = m_Components->create_item();
    const InspectedEntity inspected = m_Inspector->Inspect(entity);
    for (const InspectedComponent &component : inspected.components) {
        godot::TreeItem *item = m_Components->create_item(root);
        item->set_text(kNameColumn, ToGodot(component.name));
        item->set_text(kValueColumn, ToGodot(component.value.empty() ? kNoValue : component.value));
    }
}

} // namespace Runic::GodotTools::Detail
