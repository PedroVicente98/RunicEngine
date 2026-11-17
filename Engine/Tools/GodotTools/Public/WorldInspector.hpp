#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "Entity.hpp"
#include "PlainIds.hpp"
#include "SimulationRuntime.hpp"

// The world and ECS inspectors' model (GDT-2): what the first dock shows of a local simulation in
// the editor process, as plain text, so the dock only lays it out. It reads the simulation between
// its steps, on the editor's main thread, and never changes it.

namespace Runic::GodotTools {

// The simulation as a whole.
struct WorldSummary {
    Core::SimulationId simulation;
    std::uint64_t masterTick = 0; // master ticks run so far
    std::uint32_t entities = 0;
    std::uint32_t componentTypes = 0;
    std::uint32_t queries = 0;
    std::uint32_t states = 0; // declared states
    std::uint32_t commandQueues = 0;
    std::uint32_t regions = 0; // entities that are World::Region
};

struct InspectedComponent {
    std::string name;
    std::string value; // the component's formatter's text; empty when it has none
};

struct InspectedEntity {
    ECS::Entity entity;
    std::string label;                          // "entity <n>", n stable while the entity lives
    std::vector<InspectedComponent> components; // in registration order
};

// The most entities a list holds: a dock shows a page, never a world of 100,000.
inline constexpr std::uint32_t kDefaultEntityListLimit = 500;

class WorldInspector {
  public:
    // runtime outlives the inspector.
    explicit WorldInspector(Simulation::SimulationRuntime &runtime) noexcept : m_Runtime(runtime) {}

    WorldSummary Summary() const;
    // Entities in a stable order (by handle), at most limit of them.
    std::vector<ECS::Entity> Entities(std::uint32_t limit = kDefaultEntityListLimit) const;
    // An entity's components and their values; empty components when it is gone.
    InspectedEntity Inspect(ECS::Entity entity) const;
    // "<query name> (<n> terms)", in the order the queries were made.
    std::vector<std::string> Queries() const;
    // "<queue name>: <queued> queued, <applied> applied".
    std::vector<std::string> CommandQueues() const;
    // "<name> (<category>)" for every declared state, when the states can be listed (see the
    // module's ARCHITECTURE.md: Runic::ECS has no way to list them yet, so this is empty).
    std::vector<std::string> States() const;

  private:
    Simulation::SimulationRuntime &m_Runtime;
};

} // namespace Runic::GodotTools
