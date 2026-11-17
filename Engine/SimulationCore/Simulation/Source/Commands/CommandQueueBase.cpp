// Command queues (CommandQueue.hpp): the part that does not depend on the command's type.

#include "CommandQueue.hpp"

#include <algorithm>

#include "Assert.hpp"
#include "Scheduler/RuntimeState.hpp"

namespace Runic::Simulation::Detail {

namespace {

constexpr std::string_view kCommandsAppliedHelp = "Commands of one queue applied to the world";

} // namespace

CommandQueueBase::CommandQueueBase(SimulationRuntime &runtime, std::string_view name)
    : m_Runtime(runtime), m_Name(name) {
    RUNIC_ASSERT(!name.empty());
    SimulationRuntime::State &state = *m_Runtime.m_State;
    RUNIC_ASSERT_MSG(!state.stepping, "a command queue was made while a master step runs");
    if (state.metrics != nullptr) {
        const std::string metric = std::string(kCommandsAppliedMetricPrefix) + m_Name;
        m_AppliedMetric = state.metrics->AddCounter(
            {metric, TickDomainName(TickDomain::Hz30), kCommandsAppliedHelp});
    }
    state.commandQueues.push_back(this);
}

CommandQueueBase::~CommandQueueBase() { std::erase(m_Runtime.m_State->commandQueues, this); }

CommandQueueInfo CommandQueueBase::Describe() const noexcept {
    CommandQueueInfo info;
    info.name = m_Name;
    info.queued = m_Queued.load(std::memory_order_relaxed);
    info.applied = m_Applied.load(std::memory_order_relaxed);
    return info;
}

void CommandQueueBase::CheckCommandsPhase() const noexcept {
    RUNIC_ASSERT_MSG(m_Runtime.IsRunning(RealtimePhase::Commands),
                     "commands apply in the Commands phase of the realtime slot, nowhere else");
}

void CommandQueueBase::CountApplied(std::size_t applied) noexcept {
    m_Applied.fetch_add(applied, std::memory_order_relaxed);
    if (Core::MetricsRegistry *metrics = m_Runtime.m_State->metrics) {
        metrics->Add(m_AppliedMetric, applied);
    }
}

} // namespace Runic::Simulation::Detail
