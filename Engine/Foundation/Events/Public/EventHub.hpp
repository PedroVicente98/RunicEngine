#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "EventId.hpp"
#include "PlainIds.hpp"

// The EventHub: the event buffers of one simulation and the sequence their events take their IDs
// from. The simulation flushes the hub at a barrier between phases: every buffer's emitted events
// get their IDs, in a fixed order, and become readable (RunicDocs › Simulation core clock and
// threads: events flush at barriers; a fact is visible to the next phase).

namespace Runic::Events {

class EventHub;

// A buffer as tools and tests see it. The name stays valid while the buffer lives.
struct EventBufferInfo {
    std::string_view name;
    std::size_t pending = 0;  // emitted, waiting for the next flush
    std::size_t readable = 0; // flushed and still kept for a reader
    std::uint32_t readers = 0;
    std::uint64_t flushed = 0; // events flushed since the buffer was made
    std::uint64_t dropped = 0; // events a full buffer let go before every reader had them
};

namespace Detail {

// What the hub and the readers need of a buffer whatever its event type: its place in the hub,
// the cursors of its readers and the bookkeeping of what it keeps.
class EventBufferBase {
  public:
    EventBufferBase(const EventBufferBase &) = delete;
    EventBufferBase &operator=(const EventBufferBase &) = delete;

    std::string_view Name() const noexcept { return m_Name; }
    EventBufferInfo Describe() const noexcept;

  protected:
    EventBufferBase(EventHub &hub, std::string_view name, std::size_t capacity);
    ~EventBufferBase();

    // The half-open run of buffer-local indices [first, end) a reader has not read.
    struct Unread {
        std::uint64_t first = 0;
        std::uint64_t count = 0;
    };

    std::uint32_t AddReader();
    void RemoveReader(std::uint32_t reader) noexcept;
    // What the reader has not read; marks it read.
    Unread TakeUnread(std::uint32_t reader) noexcept;
    std::uint64_t MissedBy(std::uint32_t reader) const noexcept;

    // Called by Flush after it appended `arrived` events to the kept ones. Returns how many of the
    // oldest kept events to let go: the ones every reader has read and, when the buffer is over
    // its capacity, the oldest beyond it (the readers that had not read those are told how many
    // they missed).
    std::size_t EndFlush(std::size_t arrived);

    // The buffer-local index of the oldest event kept.
    std::uint64_t FirstKept() const noexcept { return m_FirstKept; }
    Core::SimulationId Simulation() const noexcept;
    EventSequence &Sequence() noexcept;
    void SetPending(std::size_t pending) noexcept { m_Pending = pending; }

  private:
    friend class Runic::Events::EventHub;

    // Gives the pending events their IDs and makes them readable; returns how many.
    virtual std::size_t Flush() = 0;

    struct Reader {
        std::uint64_t cursor = 0; // the next index to read
        std::uint64_t missed = 0;
        bool active = false;
    };

    EventHub &m_Hub;
    std::string m_Name;
    std::size_t m_Capacity;
    std::vector<Reader> m_Readers;
    std::uint64_t m_FirstKept = 0; // local index of the oldest event kept
    std::uint64_t m_End = 0;       // local index one past the newest readable event
    std::size_t m_Pending = 0;
    std::uint64_t m_Dropped = 0;
    bool m_Overflowing = false;
};

} // namespace Detail

class EventHub {
  public:
    // A simulation's hub. next is where the sequence starts: kFirstEventSequence for a simulation
    // that starts from nothing, or the value a checkpoint saved.
    explicit EventHub(Core::SimulationId simulation,
                      std::uint64_t nextSequence = kFirstEventSequence);
    // Every buffer of the hub has been destroyed.
    ~EventHub();

    EventHub(const EventHub &) = delete;
    EventHub &operator=(const EventHub &) = delete;

    Core::SimulationId Simulation() const noexcept { return m_Sequence.Simulation(); }
    // The number the next flushed event gets: what a checkpoint saves.
    std::uint64_t NextSequence() const noexcept { return m_Sequence.Next(); }

    // The barrier: every buffer, in the order the buffers were made, gives its emitted events
    // their IDs in the order they were emitted and makes them readable; events every reader has
    // read are let go. Returns how many events became readable. Called by the simulation's
    // driving thread while no system runs.
    std::size_t Flush();

    // The buffers, in the order they were made, for tools.
    std::uint32_t BufferCount() const noexcept;
    EventBufferInfo Describe(std::uint32_t index) const;

  private:
    friend class Detail::EventBufferBase;

    EventSequence m_Sequence;
    std::vector<Detail::EventBufferBase *> m_Buffers;
};

} // namespace Runic::Events
