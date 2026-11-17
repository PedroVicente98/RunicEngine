// Event IDs (EventId.hpp): the sequence of a simulation and an ID as text.

#include "EventId.hpp"

#include <format>

#include "Assert.hpp"

namespace Runic::Events {

std::string ToText(const EventId &id) {
    return std::format("{}:{}", id.simulation.Value(), id.sequence);
}

EventSequence::EventSequence(Core::SimulationId simulation, std::uint64_t next) noexcept
    : m_Simulation(simulation), m_Next(next) {
    RUNIC_ASSERT_MSG(simulation.IsValid(), "an event sequence needs a valid SimulationId");
    RUNIC_ASSERT_MSG(next != kNoEventSequence, "an event sequence starts at 1 or later");
}

EventId EventSequence::Take(std::uint64_t count) noexcept {
    const EventId first{m_Simulation, m_Next};
    m_Next += count;
    return first;
}

} // namespace Runic::Events
