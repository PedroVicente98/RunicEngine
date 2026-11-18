// The inspectors' model (WorldInspector.hpp).

#include "WorldInspector.hpp"

#include <algorithm>
#include <format>

#include "EntityWorld.hpp"
#include "InspectorText.hpp"
#include "WorldComponents.hpp"

namespace Runic::GodotTools {

WorldSummary WorldInspector::Summary() const {
    const ECS::EntityWorld &world = m_Runtime.Entities();
    WorldSummary summary;
    summary.simulation = m_Runtime.Id();
    summary.masterTick = m_Runtime.NextMasterTick().Value();
    summary.entities = world.EntityCount();
    summary.componentTypes = world.ComponentCount();
    summary.queries = world.QueryCount();
    summary.states = m_Runtime.States().Count();
    summary.commandQueues = m_Runtime.CommandQueueCount();
    if (world.IsRegistered<World::Region>()) {
        world.ForEachEntity([&](ECS::Entity entity) {
            summary.regions += world.Has<World::Region>(entity) ? 1 : 0;
        });
    }
    return summary;
}

std::vector<ECS::Entity> WorldInspector::Entities(std::uint32_t limit) const {
    std::vector<ECS::Entity> entities;
    m_Runtime.Entities().ForEachEntity([&](ECS::Entity entity) { entities.push_back(entity); });
    std::sort(entities.begin(), entities.end(),
              [](ECS::Entity a, ECS::Entity b) { return a.Raw() < b.Raw(); });
    if (entities.size() > limit) {
        entities.resize(limit);
    }
    return entities;
}

InspectedEntity WorldInspector::Inspect(ECS::Entity entity) const {
    const ECS::EntityWorld &world = m_Runtime.Entities();
    InspectedEntity inspected;
    inspected.entity = entity;
    inspected.label = Detail::EntityLabel(entity.Raw());
    if (!world.IsAlive(entity)) {
        return inspected;
    }
    world.ForEachComponentOf(entity, [&](const ECS::ComponentInfo &info, const void *value) {
        InspectedComponent component;
        component.name = info.name;
        if (info.format != nullptr && value != nullptr) {
            info.format(value, component.value);
        }
        inspected.components.push_back(std::move(component));
    });
    return inspected;
}

std::vector<std::string> WorldInspector::Queries() const {
    const ECS::EntityWorld &world = m_Runtime.Entities();
    std::vector<std::string> queries;
    for (std::uint32_t i = 0; i < world.QueryCount(); ++i) {
        const ECS::QueryInfo info = world.DescribeQuery(i);
        queries.push_back(std::format("{} ({} terms)", info.name, info.terms));
    }
    return queries;
}

std::vector<std::string> WorldInspector::CommandQueues() const {
    std::vector<std::string> queues;
    for (std::uint32_t i = 0; i < m_Runtime.CommandQueueCount(); ++i) {
        const Simulation::CommandQueueInfo info = m_Runtime.DescribeCommandQueue(i);
        queues.push_back(
            std::format("{}: {} queued, {} applied", info.name, info.queued, info.applied));
    }
    return queues;
}

std::vector<std::string> WorldInspector::States() const {
    return {}; // Runic::ECS offers no way to list declared states by index (reported)
}

} // namespace Runic::GodotTools
