// Phases and barriers (SimulationRuntime.hpp): a phase is a run of systems with the world's
// shape held still, and the barrier after it is where held-back changes apply and facts flush.

#include "Scheduler/RuntimeState.hpp"

namespace Runic::Simulation {

Core::Duration SimulationRuntime::State::RunPhase(const std::vector<Detail::System> &systems,
                                                  const TickContext &context) {
    Core::Duration elapsed{};
    {
        // Deferred mutation: while the phase runs no entity is added to, taken from or moved
        // between storages; the scope's end applies what the systems asked for, in order.
        const ECS::EntityWorld::DeferScope stable(entities);
        if (metrics == nullptr) {
            for (const Detail::System &system : systems) {
                system.run(context);
            }
        } else {
            Core::TimePoint before = Core::MonotonicNow();
            for (const Detail::System &system : systems) {
                system.run(context);
                const Core::TimePoint after = Core::MonotonicNow();
                metrics->Record(system.time, after - before);
                elapsed += after - before;
                before = after;
            }
        }
    }
    // The facts of this phase, IDs given in a fixed order, readable from the next phase on.
    hub.Flush();
    return elapsed;
}

void SimulationRuntime::State::Barrier() {
    // Nothing is held back between phases; what remains is to flush what was emitted outside a
    // phase (between steps, by code that runs at a safe point).
    hub.Flush();
}

} // namespace Runic::Simulation
