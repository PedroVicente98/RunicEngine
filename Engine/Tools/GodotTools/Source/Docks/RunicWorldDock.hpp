#pragma once

#include <memory>

#include <godot_cpp/classes/item_list.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/tree.hpp>
#include <godot_cpp/classes/v_box_container.hpp>

#include "WorldInspector.hpp"

// The first Runic dock (GDT-2): the world and ECS inspectors over the editor's local simulation.
// The top shows the simulation (its ID, tick, entity, component, query and queue counts), then the
// entities, then the selected entity's components with their values, then the queries and command
// queues. It refreshes a few times a second from a WorldInspector and never changes the world. It
// takes its look from the editor theme, like every Runic tool. Private to Runic::GodotTools.

namespace Runic::GodotTools::Detail {

class RunicWorldDock : public godot::VBoxContainer {
    GDCLASS(RunicWorldDock, godot::VBoxContainer)

  public:
    // The dock's name in the editor, and its node name.
    static constexpr const char *kTitle = "Runic World";

    void Attach(Simulation::SimulationRuntime &runtime);
    void _ready() override;
    void _process(double delta) override;

    // Rebuilds every list from the simulation now.
    void Refresh();
    std::uint32_t ShownEntities() const noexcept { return m_ShownEntities; }

  protected:
    static void _bind_methods() {}

  private:
    void OnEntitySelected();
    void ShowEntity(ECS::Entity entity);

    std::unique_ptr<WorldInspector> m_Inspector;
    godot::Label *m_Summary = nullptr;
    godot::Tree *m_Entities = nullptr;
    godot::Tree *m_Components = nullptr;
    godot::ItemList *m_Queries = nullptr;
    ECS::Entity m_Selected;
    double m_SinceRefresh = 0.0;
    std::uint32_t m_ShownEntities = 0;
    bool m_Reported = false;
};

} // namespace Runic::GodotTools::Detail
