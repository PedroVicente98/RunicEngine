// The SimulationRuntime (SimulationRuntime.hpp): the master step, the slots and the domain
// metrics.

#include "SimulationRuntime.hpp"

#include <array>
#include <string>
#include <vector>

#include "Assert.hpp"
#include "Scheduler/RuntimeState.hpp"
#include "WorldComponents.hpp"

namespace Runic::Simulation {

namespace {

constexpr std::string_view kTickTimeHelp = "Time the systems of one clock domain took in a tick";
constexpr std::string_view kTicksHelp = "Ticks a clock domain has run";
constexpr std::string_view kStepTimeHelp = "Time one 60 Hz master step took, all domains";
constexpr std::string_view kOverrunsHelp = "Master steps that took longer than their 1/60 s";
constexpr std::string_view kSystemTimeHelp = "Time one system took in a tick";

// The slots in the order a master step runs them.
constexpr std::array<ScheduleSlot, kScheduleSlotCount> kSlotOrder{
    ScheduleSlot::Hz20Producers, ScheduleSlot::Hz10Producers, ScheduleSlot::Hz30Realtime,
    ScheduleSlot::Hz20Consumers, ScheduleSlot::Hz10Consumers, ScheduleSlot::Hz60Hook};

// The realtime slot's phases in the order they run.
constexpr std::array<RealtimePhase, kRealtimePhaseCount> kPhaseOrder{
    RealtimePhase::Commands,    RealtimePhase::Movement,    RealtimePhase::PhysicsStructure,
    RealtimePhase::PhysicsStep, RealtimePhase::PostPhysics, RealtimePhase::Queries,
    RealtimePhase::Commit};

} // namespace

SimulationRuntime::State::State(const SimulationSpec &spec)
    : id(spec.id), metrics(spec.metrics), workers(Core::WorkerPoolSpec{spec.workers}),
      states(entities), hub(spec.id), stateEntered(hub, kStateEnteredBufferName),
      stateExited(hub, kStateExitedBufferName), stateFacts(stateEntered, stateExited) {
    World::RegisterWorldComponents(entities);
    states.SetFactSink(&stateFacts);
}

SimulationRuntime::SimulationRuntime(const SimulationSpec &spec) {
    RUNIC_ASSERT_MSG(spec.id.IsValid(), "a simulation needs a valid SimulationId");
    m_State = std::make_unique<State>(spec);
    if (Core::MetricsRegistry *metrics = spec.metrics) {
        for (const TickDomain domain : kTickDomains) {
            const std::string_view label = TickDomainName(domain);
            m_State->tickTime[IndexOf(domain)] =
                metrics->AddTiming({kTickTimeMetric, label, kTickTimeHelp});
            m_State->ticks[IndexOf(domain)] =
                metrics->AddCounter({kTicksMetric, label, kTicksHelp});
        }
        m_State->stepTime =
            metrics->AddTiming({kStepTimeMetric, Core::kNoMetricDomain, kStepTimeHelp});
        m_State->overruns =
            metrics->AddCounter({kOverrunsMetric, Core::kNoMetricDomain, kOverrunsHelp});
    }
}

SimulationRuntime::~SimulationRuntime() {
    RUNIC_ASSERT_MSG(m_State->commandQueues.empty(),
                     "a SimulationRuntime was destroyed before its command queues");
}

void SimulationRuntime::AddSystem(const SystemSpec &spec) {
    RUNIC_ASSERT_MSG(!m_State->stepping, "a system was added while a master step runs");
    RUNIC_ASSERT(!spec.name.empty());
    RUNIC_ASSERT(spec.run != nullptr);
    Detail::System system;
    system.name = spec.name;
    system.run = spec.run;
    if (m_State->metrics != nullptr) {
        const std::string metric = std::string(kSystemTimeMetricPrefix) + system.name;
        system.time = m_State->metrics->AddTiming(
            {metric, TickDomainName(DomainOf(spec.slot)), kSystemTimeHelp});
    }
    if (spec.slot == ScheduleSlot::Hz30Realtime) {
        m_State->realtime[static_cast<std::size_t>(spec.phase)].push_back(std::move(system));
    } else {
        m_State->slots[static_cast<std::size_t>(spec.slot)].push_back(std::move(system));
    }
}

void SimulationRuntime::RunMasterTick() {
    State &state = *m_State;
    RUNIC_ASSERT_MSG(!state.stepping, "RunMasterTick was called from inside a master step");
    state.stepping = true;

    const bool timed = state.metrics != nullptr;
    const Core::TickId tick = state.nextTick;
    const Core::TimePoint stepStart = timed ? Core::MonotonicNow() : Core::TimePoint{};
    std::array<Core::Duration, kTickDomainCount> domainTime{};

    // What was emitted between steps is readable in this step's first phase.
    state.Barrier();

    for (const ScheduleSlot slot : kSlotOrder) {
        const TickDomain domain = DomainOf(slot);
        if (!IsDue(domain, tick)) {
            continue;
        }
        TickContext context;
        context.domain = domain;
        context.masterTick = tick;
        context.domainTick = DomainTickAt(domain, tick);
        context.dt = StepSeconds(domain);
        context.time = RateOf(domain).TimeOfTick(context.domainTick);

        if (slot != ScheduleSlot::Hz30Realtime) {
            const std::vector<Detail::System> &systems =
                state.slots[static_cast<std::size_t>(slot)];
            if (!systems.empty()) {
                domainTime[IndexOf(domain)] += state.RunPhase(systems, context);
            }
            continue;
        }
        for (const RealtimePhase phase : kPhaseOrder) {
            const std::vector<Detail::System> &systems =
                state.realtime[static_cast<std::size_t>(phase)];
            if (systems.empty()) {
                continue;
            }
            state.runningPhase = phase;
            domainTime[IndexOf(domain)] += state.RunPhase(systems, context);
            state.runningPhase.reset();
        }
    }

    for (const TickDomain domain : kTickDomains) {
        if (!IsDue(domain, tick)) {
            continue;
        }
        ++state.ticksRun[IndexOf(domain)];
        if (timed) {
            state.metrics->Record(state.tickTime[IndexOf(domain)], domainTime[IndexOf(domain)]);
            state.metrics->Increment(state.ticks[IndexOf(domain)]);
        }
    }
    if (timed) {
        const Core::Duration stepTime = Core::MonotonicNow() - stepStart;
        state.metrics->Record(state.stepTime, stepTime);
        // The time from one master tick to the next: a step that takes longer cannot keep up.
        const Core::Duration interval = RateOf(TickDomain::Hz60).TimeOfTick(tick.Value() + 1) -
                                        RateOf(TickDomain::Hz60).TimeOfTick(tick.Value());
        if (stepTime > interval) {
            state.metrics->Increment(state.overruns);
        }
    }

    state.nextTick = tick.Next();
    state.stepping = false;
}

ECS::EntityWorld &SimulationRuntime::Entities() noexcept { return m_State->entities; }

ECS::EntityStates &SimulationRuntime::States() noexcept { return m_State->states; }

Events::EventHub &SimulationRuntime::EventHub() noexcept { return m_State->hub; }

Events::EventBuffer<ECS::StateEntered> &SimulationRuntime::StateEnteredFacts() noexcept {
    return m_State->stateEntered;
}

Events::EventBuffer<ECS::StateExited> &SimulationRuntime::StateExitedFacts() noexcept {
    return m_State->stateExited;
}

bool SimulationRuntime::IsRunning(RealtimePhase phase) const noexcept {
    return m_State->runningPhase == phase;
}

std::uint32_t SimulationRuntime::CommandQueueCount() const noexcept {
    return static_cast<std::uint32_t>(m_State->commandQueues.size());
}

CommandQueueInfo SimulationRuntime::DescribeCommandQueue(std::uint32_t index) const {
    RUNIC_ASSERT(index < m_State->commandQueues.size());
    return m_State->commandQueues[index]->Describe();
}

Core::SimulationId SimulationRuntime::Id() const noexcept { return m_State->id; }

Core::TickId SimulationRuntime::NextMasterTick() const noexcept { return m_State->nextTick; }

std::uint64_t SimulationRuntime::TicksRun(TickDomain domain) const noexcept {
    return m_State->ticksRun[IndexOf(domain)];
}

Core::Duration SimulationRuntime::Time() const noexcept {
    return RateOf(TickDomain::Hz60).TimeOfTick(m_State->nextTick.Value());
}

Core::WorkerPool &SimulationRuntime::Workers() noexcept { return m_State->workers; }

Core::MetricsRegistry *SimulationRuntime::Metrics() const noexcept { return m_State->metrics; }

} // namespace Runic::Simulation
