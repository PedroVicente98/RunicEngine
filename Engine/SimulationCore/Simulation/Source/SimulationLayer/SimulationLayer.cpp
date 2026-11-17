// The Simulation layer (SimulationLayer.hpp): owed master steps from the loop's time.

#include "SimulationLayer.hpp"

#include <algorithm>
#include <string>
#include <utility>

#include "Assert.hpp"
#include "Log.hpp"

namespace Runic::Simulation {

namespace {

constexpr std::string_view kBacklogHelp = "Master steps the simulation owes after an update";

// How often a simulation that stays behind says so: on the first update that ends with a
// backlog, then once every this many updates in a row.
constexpr std::uint64_t kUpdatesBetweenBacklogWarnings = 300;

} // namespace

SimulationLayer::SimulationLayer(std::unique_ptr<SimulationRuntime> runtime,
                                 const SimulationLayerSpec &spec)
    : Layer(std::string(kSimulationLayerName)), m_Runtime(std::move(runtime)), m_Spec(spec) {
    RUNIC_ASSERT(m_Runtime != nullptr);
    RUNIC_ASSERT(m_Spec.maxTicksPerUpdate > 0);
    if (Core::MetricsRegistry *metrics = m_Runtime->Metrics()) {
        m_Backlog =
            metrics->AddGauge({kBacklogMetric, TickDomainName(TickDomain::Hz60), kBacklogHelp});
    }
}

SimulationLayer::~SimulationLayer() = default;

SimulationRuntime &SimulationLayer::Runtime() noexcept { return *m_Runtime; }

const SimulationRuntime &SimulationLayer::Runtime() const noexcept { return *m_Runtime; }

std::uint64_t SimulationLayer::BacklogTicks() const noexcept { return m_BacklogTicks; }

Core::Result<void> SimulationLayer::OnAttach(Application::LayerContext &) {
    RUNIC_LOG_INFO("RUNIC: {} layer attached: simulation {}, {} worker(s), up to {} step(s) per "
                   "update",
                   Name(), m_Runtime->Id().Value(), m_Runtime->Workers().WorkerCount(),
                   m_Spec.maxTicksPerUpdate);
    return {};
}

void SimulationLayer::OnUpdate(const Application::FrameTime &time) {
    if (!m_Started) {
        m_Started = true;
        m_Start = time.sinceStart;
    }
    // Master tick k is due at TimeOfTick(k), counted from the first update: the ticks whose time
    // has come are 0 to TicksIn(elapsed), from integers only, so no error ever adds up.
    const Core::Duration elapsed = time.sinceStart - m_Start;
    const std::uint64_t due = RateOf(TickDomain::Hz60).TicksIn(elapsed) + 1;
    const std::uint64_t owed = due > m_TicksRun ? due - m_TicksRun : 0;
    const std::uint64_t steps = std::min<std::uint64_t>(owed, m_Spec.maxTicksPerUpdate);
    for (std::uint64_t step = 0; step < steps; ++step) {
        m_Runtime->RunMasterTick();
    }
    m_TicksRun += steps;
    m_BacklogTicks = owed - steps;

    if (Core::MetricsRegistry *metrics = m_Runtime->Metrics()) {
        metrics->Set(m_Backlog, static_cast<double>(m_BacklogTicks));
    }
    if (m_BacklogTicks == 0) {
        m_UpdatesBehind = 0;
        return;
    }
    // Falling behind is measured and alarmed, never skipped: the owed steps run in later updates.
    if (m_UpdatesBehind % kUpdatesBetweenBacklogWarnings == 0) {
        RUNIC_LOG_WARNING("RUNIC: simulation {} is behind: {} master step(s) owed after running {} "
                          "in one update",
                          m_Runtime->Id().Value(), m_BacklogTicks, steps);
    }
    ++m_UpdatesBehind;
}

void SimulationLayer::OnDetach() {
    RUNIC_LOG_INFO("RUNIC: {} layer ran {} master ticks ({} {}, {} {}, {} {}, {} {}), {} owed",
                   Name(), m_Runtime->NextMasterTick().Value(), TickDomainName(TickDomain::Hz60),
                   m_Runtime->TicksRun(TickDomain::Hz60), TickDomainName(TickDomain::Hz30),
                   m_Runtime->TicksRun(TickDomain::Hz30), TickDomainName(TickDomain::Hz20),
                   m_Runtime->TicksRun(TickDomain::Hz20), TickDomainName(TickDomain::Hz10),
                   m_Runtime->TicksRun(TickDomain::Hz10), m_BacklogTicks);
}

} // namespace Runic::Simulation
