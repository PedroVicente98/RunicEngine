#pragma once

#include <compare>
#include <cstdint>
#include <string>

#include "PlainIds.hpp"

// The ID of an event (a fact a simulation produced): the simulation and a sequence number. The
// sequence is drawn in a fixed order when events are flushed, so a simulation replayed from the
// same state gives every event the same ID again, and a durable consumer can tell an event it has
// already handled from a new one (RunicDocs › Simulation core › Flecs rules: hot facts travel in
// event buffers with stable event IDs).

namespace Runic::Events {

// The sequence of the first event of a simulation that starts from nothing. 0 is no event.
inline constexpr std::uint64_t kNoEventSequence = 0;
inline constexpr std::uint64_t kFirstEventSequence = 1;

struct EventId {
    Core::SimulationId simulation;
    std::uint64_t sequence = kNoEventSequence;

    constexpr bool IsValid() const noexcept {
        return simulation.IsValid() && sequence != kNoEventSequence;
    }

    friend constexpr bool operator==(const EventId &, const EventId &) noexcept = default;
    friend constexpr std::strong_ordering operator<=>(const EventId &left,
                                                      const EventId &right) noexcept {
        if (const auto order = left.simulation.Value() <=> right.simulation.Value(); order != 0) {
            return order;
        }
        return left.sequence <=> right.sequence;
    }
};

// "<simulation>:<sequence>", for logs and for a consumer's record of what it has handled.
std::string ToText(const EventId &id);

// The sequence numbers of one simulation: where the next event's number comes from. A checkpoint
// saves Next(), and a simulation restored from it starts its sequence there, which is what keeps
// the IDs of replayed events the same.
class EventSequence {
  public:
    explicit EventSequence(Core::SimulationId simulation,
                           std::uint64_t next = kFirstEventSequence) noexcept;

    Core::SimulationId Simulation() const noexcept { return m_Simulation; }
    // The number the next event gets.
    std::uint64_t Next() const noexcept { return m_Next; }

    // Takes count numbers in a row and returns the ID of the first.
    EventId Take(std::uint64_t count) noexcept;

  private:
    Core::SimulationId m_Simulation;
    std::uint64_t m_Next;
};

} // namespace Runic::Events
