#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "CommandQueue.hpp"
#include "ParallelWork.hpp"
#include "PhysicsBridge.hpp"
#include "SimulationRuntime.hpp"

// The state behind a SimulationRuntime, shared by the files that implement it: the scheduler
// (Scheduler/), the phases and their barriers (Phases/), the command queues (Commands/), the
// physics bridge (PhysicsBridge/) and the parallel loops (Workers/).

namespace Runic::Simulation {

namespace Detail {

struct System {
    std::string name;
    SystemFunction run;
    Core::TimingId time;
};

// Puts the facts of observable states into the simulation's event buffers.
class StateFactsToEvents final : public ECS::StateFactSink {
  public:
    StateFactsToEvents(Events::EventBuffer<ECS::StateEntered> &entered,
                       Events::EventBuffer<ECS::StateExited> &exited) noexcept
        : m_Entered(entered), m_Exited(exited) {}

    void OnStateEntered(const ECS::StateEntered &fact) override { m_Entered.Emit(fact); }
    void OnStateExited(const ECS::StateExited &fact) override { m_Exited.Emit(fact); }

  private:
    Events::EventBuffer<ECS::StateEntered> &m_Entered;
    Events::EventBuffer<ECS::StateExited> &m_Exited;
};

} // namespace Detail

struct SimulationRuntime::State {
    explicit State(const SimulationSpec &spec);

    const Core::SimulationId id;
    Core::MetricsRegistry *const metrics;
    Core::WorkerPool workers;

    // In this order: the buffers leave the hub, and the states their world, before those go.
    ECS::EntityWorld entities;
    ECS::EntityStates states;
    Events::EventHub hub;
    Events::EventBuffer<ECS::StateEntered> stateEntered;
    Events::EventBuffer<ECS::StateExited> stateExited;
    Detail::StateFactsToEvents stateFacts;

    // The systems: the realtime slot's by phase, every other slot's in one list.
    std::array<std::vector<Detail::System>, kScheduleSlotCount> slots;
    std::array<std::vector<Detail::System>, kRealtimePhaseCount> realtime;
    std::vector<Detail::CommandQueueBase *> commandQueues;

    Core::TickId nextTick;
    std::array<std::uint64_t, kTickDomainCount> ticksRun{};
    bool stepping = false;
    std::optional<RealtimePhase> runningPhase;
    Detail::ParallelLoops parallel; // Workers/: the loop that runs and the next loop's number

    std::array<Core::TimingId, kTickDomainCount> tickTime;
    std::array<Core::CounterId, kTickDomainCount> ticks;
    Core::TimingId stepTime;
    Core::CounterId overruns;

    // The physics bridge, once enabled. Last: its systems, queries and fact buffer go before the
    // world and the hub they belong to.
    std::unique_ptr<PhysicsBridge> physics;

    // Phases/: runs one phase's systems with the world's changes held back, then the barrier.
    // Returns the time the systems took (zero without a metrics registry).
    Core::Duration RunPhase(const std::vector<Detail::System> &systems, const TickContext &context);
    // Applies what was held back and makes emitted facts readable.
    void Barrier();
    // After held-back changes applied: bodies and characters follow their destroyed entities.
    void FollowEntities();
};

} // namespace Runic::Simulation
