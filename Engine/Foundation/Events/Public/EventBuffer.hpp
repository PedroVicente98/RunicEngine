#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

#include "Assert.hpp"
#include "EventHub.hpp"
#include "EventId.hpp"

// Typed event buffers and their readers. A system emits facts into a buffer during a phase; the
// simulation flushes its hub at the barrier; from then on every reader of the buffer gets each
// fact exactly once, with its ID. A buffer is tick-safe: what is emitted is not readable until
// the flush, so nothing a reader sees changes while a phase runs, and readers of different rates
// (a 10 Hz consumer of 30 Hz facts) each read at their own pace.

namespace Runic::Events {

// Events a buffer keeps for its readers unless its constructor is given another capacity. A
// reader that falls further behind misses the oldest ones, and is told how many.
inline constexpr std::size_t kDefaultEventCapacity = 16'384;

// A flushed event: the fact and its ID.
template <class T> struct Event {
    EventId id;
    T fact;
};

template <class T> class EventReader;

template <class T> class EventBuffer final : public Detail::EventBufferBase {
  public:
    // Joins the hub: the buffer flushes with it, after the buffers made before it. The hub
    // outlives the buffer. The name is for tools ("engine:test_hits").
    EventBuffer(EventHub &hub, std::string_view name, std::size_t capacity = kDefaultEventCapacity)
        : EventBufferBase(hub, name, capacity) {}

    // Records a fact. It becomes readable, with its ID, at the hub's next flush. One thread
    // emits into a buffer at a time: the simulation's driving thread, in a phase.
    void Emit(const T &fact) {
        m_Pending.push_back(fact);
        SetPending(m_Pending.size());
    }
    void Emit(T &&fact) {
        m_Pending.push_back(std::move(fact));
        SetPending(m_Pending.size());
    }

    // A reader that gets every event flushed from now on. It must not outlive the buffer.
    EventReader<T> MakeReader() { return EventReader<T>(*this, AddReader()); }

  private:
    friend class EventReader<T>;

    std::size_t Flush() override {
        const std::size_t arriving = m_Pending.size();
        EventId id = Sequence().Take(arriving);
        for (T &fact : m_Pending) {
            m_Events.push_back(Event<T>{id, std::move(fact)});
            ++id.sequence;
        }
        m_Pending.clear();
        SetPending(m_Pending.size());
        // Let go of what every reader has read (and of the oldest events of a buffer over its
        // capacity), then of their storage once it is most of the array.
        m_Head += EndFlush(arriving);
        if (m_Head > 0 && m_Head >= m_Events.size() - m_Head) {
            m_Events.erase(m_Events.begin(),
                           m_Events.begin() + static_cast<std::ptrdiff_t>(m_Head));
            m_Head = 0;
        }
        return arriving;
    }

    std::span<const Event<T>> Take(std::uint32_t reader) noexcept {
        const Unread unread = TakeUnread(reader);
        return std::span<const Event<T>>(m_Events.data() + m_Head +
                                             static_cast<std::size_t>(unread.first - FirstKept()),
                                         static_cast<std::size_t>(unread.count));
    }

    std::vector<T> m_Pending;       // emitted since the last flush
    std::vector<Event<T>> m_Events; // flushed; the kept ones start at m_Head
    std::size_t m_Head = 0;
};

// One consumer's place in a buffer. Each reader gets every event flushed after it was made,
// once, in order, whatever the other readers do. One thread uses a reader at a time; several
// readers may read at once, between two flushes.
template <class T> class EventReader {
  public:
    // A reader of nothing: EventBuffer::MakeReader gives the real ones.
    EventReader() noexcept = default;
    EventReader(EventReader &&other) noexcept
        : m_Buffer(std::exchange(other.m_Buffer, nullptr)), m_Reader(other.m_Reader) {}
    EventReader &operator=(EventReader &&other) noexcept {
        if (this != &other) {
            Release();
            m_Buffer = std::exchange(other.m_Buffer, nullptr);
            m_Reader = other.m_Reader;
        }
        return *this;
    }
    EventReader(const EventReader &) = delete;
    EventReader &operator=(const EventReader &) = delete;
    // Leaves the buffer, which stops keeping events for it.
    ~EventReader() { Release(); }

    bool IsValid() const noexcept { return m_Buffer != nullptr; }

    // The events flushed since the last Take, oldest first, now marked read. The span is valid
    // until the hub's next flush.
    std::span<const Event<T>> Take() noexcept {
        RUNIC_ASSERT_MSG(IsValid(), "a reader of nothing was read");
        return m_Buffer->Take(m_Reader);
    }

    // Events the buffer let go before this reader had read them, because it fell further behind
    // than the buffer's capacity. A durable consumer treats any as a fault.
    std::uint64_t Missed() const noexcept {
        RUNIC_ASSERT_MSG(IsValid(), "a reader of nothing was asked what it missed");
        return m_Buffer->MissedBy(m_Reader);
    }

  private:
    friend class EventBuffer<T>;

    EventReader(EventBuffer<T> &buffer, std::uint32_t reader) noexcept
        : m_Buffer(&buffer), m_Reader(reader) {}

    void Release() noexcept {
        if (m_Buffer != nullptr) {
            m_Buffer->RemoveReader(m_Reader);
            m_Buffer = nullptr;
        }
    }

    EventBuffer<T> *m_Buffer = nullptr;
    std::uint32_t m_Reader = 0;
};

} // namespace Runic::Events
