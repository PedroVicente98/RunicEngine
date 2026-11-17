#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string_view>

#include "MetricsRegistry.hpp"
#include "PlainIds.hpp"
#include "TickContext.hpp"
#include "TickDomain.hpp"
#include "Time.hpp"
#include "WorkerPool.hpp"

// The SimulationRuntime: one simulation's clock and scheduler. It advances in exact 60 Hz steps;
// each step runs the systems of the domains that are due, in a fixed order of slots (RunicDocs ›
// Simulation core clock and threads). It owns the simulation's worker pool. Wall-clock time is
// not its business: whoever drives it (the Simulation layer) decides how many steps are owed.

namespace Runic::Simulation {

// Where in a master step a system runs. Rates decide when a system is due; the slots decide
// order: the 20 and 10 Hz producers, then the 30 Hz realtime phase, then the 20 and 10 Hz
// consumers, then the 60 Hz hook. Inside a slot, systems run in the order they were added.
enum class ScheduleSlot : std::uint8_t {
    Hz20Producers, // AI decisions and perception: what the realtime phase will act on
    Hz10Producers, // slow scans and world state the realtime phase reads
    Hz30Realtime,  // commands, movement, physics, combat queries, replication
    Hz20Consumers, // 20 Hz work that reads what the realtime phase produced
    Hz10Consumers, // quest and loot consumers, spawning, maintenance
    Hz60Hook,      // the master clock's hook; no mandatory gameplay work
};

inline constexpr std::size_t kScheduleSlotCount = 6;

namespace Detail {
inline constexpr std::array<TickDomain, kScheduleSlotCount> kSlotDomains{
    TickDomain::Hz20, TickDomain::Hz10, TickDomain::Hz30,
    TickDomain::Hz20, TickDomain::Hz10, TickDomain::Hz60};
inline constexpr std::array<std::string_view, kScheduleSlotCount> kSlotNames{
    "hz20 producers", "hz10 producers", "hz30 realtime",
    "hz20 consumers", "hz10 consumers", "hz60 hook"};
} // namespace Detail

constexpr TickDomain DomainOf(ScheduleSlot slot) noexcept {
    return Detail::kSlotDomains[static_cast<std::size_t>(slot)];
}

// "hz20 producers", "hz10 producers", "hz30 realtime", "hz20 consumers", "hz10 consumers" or
// "hz60 hook", for logs.
constexpr std::string_view ScheduleSlotName(ScheduleSlot slot) noexcept {
    return Detail::kSlotNames[static_cast<std::size_t>(slot)];
}

// A system: work that runs once per tick of its slot's domain. It may use the runtime's worker
// pool and must have finished its jobs when it returns.
using SystemFunction = std::function<void(const TickContext &)>;

struct SystemSpec {
    std::string_view name; // for metrics and logs, such as "engine:test_system"
    ScheduleSlot slot = ScheduleSlot::Hz30Realtime;
    SystemFunction run;
};

// The simulation of a process that hosts one and was given no ID by a service.
inline constexpr Core::SimulationId kStandaloneSimulation{1};

struct SimulationSpec {
    Core::SimulationId id = kStandaloneSimulation;
    // Worker slots of the simulation's pool, the driving thread included. One worker creates no
    // thread, which is how early slices run.
    std::uint32_t workers = Core::kSingleWorker;
    // Where the runtime records its metrics; nullptr records nothing. It must outlive the runtime.
    Core::MetricsRegistry *metrics = nullptr;
};

// The metrics a runtime records (names in the registry; the domain label is TickDomainName).
inline constexpr std::string_view kTickTimeMetric = "simulation.tick_time"; // per domain
inline constexpr std::string_view kTicksMetric = "simulation.ticks";        // per domain
inline constexpr std::string_view kStepTimeMetric = "simulation.step_time"; // one master step
inline constexpr std::string_view kOverrunsMetric = "simulation.overruns";  // steps over 1/60 s
// Per system, followed by the system's name, with its domain's label.
inline constexpr std::string_view kSystemTimeMetricPrefix = "simulation.system_time.";

class SimulationRuntime {
  public:
    explicit SimulationRuntime(const SimulationSpec &spec = {});
    ~SimulationRuntime();

    SimulationRuntime(const SimulationRuntime &) = delete;
    SimulationRuntime &operator=(const SimulationRuntime &) = delete;

    // Adds a system at the end of its slot. Between steps only: before the first, or after
    // RunMasterTick has returned.
    void AddSystem(const SystemSpec &spec);

    // Advances the simulation by exactly one 60 Hz step: every slot whose domain is due runs its
    // systems, in slot order. Called by one thread, the simulation's driving thread.
    void RunMasterTick();

    Core::SimulationId Id() const noexcept;
    // The number of the next master tick, which is also how many have run.
    Core::TickId NextMasterTick() const noexcept;
    // Ticks a domain has run. Exact for any length of run: after n master ticks, Hz30 has run
    // ⌈n / 2⌉, Hz20 ⌈n / 3⌉ and Hz10 ⌈n / 6⌉.
    std::uint64_t TicksRun(TickDomain domain) const noexcept;
    // Simulation time: the time of the next master tick, exact from its number.
    Core::Duration Time() const noexcept;

    // The simulation's worker pool, for systems that split their work.
    Core::WorkerPool &Workers() noexcept;
    // The registry given in the spec, or nullptr.
    Core::MetricsRegistry *Metrics() const noexcept;

  private:
    struct State;
    std::unique_ptr<State> m_State;
};

} // namespace Runic::Simulation
