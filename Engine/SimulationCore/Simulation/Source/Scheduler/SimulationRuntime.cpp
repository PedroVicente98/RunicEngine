// The SimulationRuntime (SimulationRuntime.hpp): the master step, the slots and the domain
// metrics.

#include "SimulationRuntime.hpp"

#include <array>
#include <string>
#include <vector>

#include "Assert.hpp"

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

struct System {
    std::string name;
    SystemFunction run;
    Core::TimingId time;
};

} // namespace

struct SimulationRuntime::State {
    explicit State(const SimulationSpec &spec)
        : id(spec.id), metrics(spec.metrics), workers(Core::WorkerPoolSpec{spec.workers}) {}

    const Core::SimulationId id;
    Core::MetricsRegistry *const metrics;
    Core::WorkerPool workers;

    std::array<std::vector<System>, kScheduleSlotCount> slots;
    Core::TickId nextTick;
    std::array<std::uint64_t, kTickDomainCount> ticksRun{};
    bool stepping = false;

    std::array<Core::TimingId, kTickDomainCount> tickTime;
    std::array<Core::CounterId, kTickDomainCount> ticks;
    Core::TimingId stepTime;
    Core::CounterId overruns;
};

SimulationRuntime::SimulationRuntime(const SimulationSpec &spec)
    : m_State(std::make_unique<State>(spec)) {
    RUNIC_ASSERT_MSG(spec.id.IsValid(), "a simulation needs a valid SimulationId");
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

SimulationRuntime::~SimulationRuntime() = default;

void SimulationRuntime::AddSystem(const SystemSpec &spec) {
    RUNIC_ASSERT_MSG(!m_State->stepping, "a system was added while a master step runs");
    RUNIC_ASSERT(!spec.name.empty());
    RUNIC_ASSERT(spec.run != nullptr);
    System system;
    system.name = spec.name;
    system.run = spec.run;
    if (m_State->metrics != nullptr) {
        const std::string metric = std::string(kSystemTimeMetricPrefix) + system.name;
        system.time = m_State->metrics->AddTiming(
            {metric, TickDomainName(DomainOf(spec.slot)), kSystemTimeHelp});
    }
    m_State->slots[static_cast<std::size_t>(spec.slot)].push_back(std::move(system));
}

void SimulationRuntime::RunMasterTick() {
    State &state = *m_State;
    RUNIC_ASSERT_MSG(!state.stepping, "RunMasterTick was called from inside a master step");
    state.stepping = true;

    const bool timed = state.metrics != nullptr;
    const Core::TickId tick = state.nextTick;
    const Core::TimePoint stepStart = timed ? Core::MonotonicNow() : Core::TimePoint{};
    std::array<Core::Duration, kTickDomainCount> domainTime{};

    for (const ScheduleSlot slot : kSlotOrder) {
        const TickDomain domain = DomainOf(slot);
        const std::vector<System> &systems = state.slots[static_cast<std::size_t>(slot)];
        if (!IsDue(domain, tick) || systems.empty()) {
            continue;
        }
        TickContext context;
        context.domain = domain;
        context.masterTick = tick;
        context.domainTick = DomainTickAt(domain, tick);
        context.dt = StepSeconds(domain);
        context.time = RateOf(domain).TimeOfTick(context.domainTick);

        if (!timed) {
            for (const System &system : systems) {
                system.run(context);
            }
            continue;
        }
        Core::TimePoint before = Core::MonotonicNow();
        for (const System &system : systems) {
            system.run(context);
            const Core::TimePoint after = Core::MonotonicNow();
            state.metrics->Record(system.time, after - before);
            domainTime[IndexOf(domain)] += after - before;
            before = after;
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
