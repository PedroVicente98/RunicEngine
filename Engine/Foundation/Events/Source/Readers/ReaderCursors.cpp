// The readers of a buffer (EventHub.hpp, Detail::EventBufferBase): their cursors, and what a
// flush may let go.

#include "EventHub.hpp"

#include <algorithm>

#include "Assert.hpp"
#include "Log.hpp"

namespace Runic::Events::Detail {

std::uint32_t EventBufferBase::AddReader() {
    // A new reader starts after everything flushed so far. A slot a reader left is used again.
    const Reader fresh{m_End, 0, true};
    for (std::uint32_t slot = 0; slot < m_Readers.size(); ++slot) {
        if (!m_Readers[slot].active) {
            m_Readers[slot] = fresh;
            return slot;
        }
    }
    m_Readers.push_back(fresh);
    return static_cast<std::uint32_t>(m_Readers.size() - 1);
}

void EventBufferBase::RemoveReader(std::uint32_t reader) noexcept {
    RUNIC_ASSERT(reader < m_Readers.size() && m_Readers[reader].active);
    m_Readers[reader].active = false;
}

EventBufferBase::Unread EventBufferBase::TakeUnread(std::uint32_t reader) noexcept {
    RUNIC_ASSERT(reader < m_Readers.size() && m_Readers[reader].active);
    Reader &state = m_Readers[reader];
    const Unread unread{state.cursor, m_End - state.cursor};
    state.cursor = m_End;
    return unread;
}

std::uint64_t EventBufferBase::MissedBy(std::uint32_t reader) const noexcept {
    RUNIC_ASSERT(reader < m_Readers.size() && m_Readers[reader].active);
    return m_Readers[reader].missed;
}

std::size_t EventBufferBase::EndFlush(std::size_t arrived) {
    m_End += arrived;

    // The oldest event some reader still has to read; with no reader, nothing is kept.
    std::uint64_t oldestUnread = m_End;
    for (const Reader &reader : m_Readers) {
        if (reader.active) {
            oldestUnread = std::min(oldestUnread, reader.cursor);
        }
    }

    // A buffer over its capacity lets go of its oldest events even though a reader lacks them.
    std::uint64_t keepFrom = oldestUnread;
    if (m_End - keepFrom > m_Capacity) {
        keepFrom = m_End - m_Capacity;
        m_Dropped += keepFrom - oldestUnread;
        for (Reader &reader : m_Readers) {
            if (reader.active && reader.cursor < keepFrom) {
                reader.missed += keepFrom - reader.cursor;
                reader.cursor = keepFrom;
            }
        }
        if (!m_Overflowing) {
            m_Overflowing = true;
            RUNIC_LOG_ERROR("RUNIC: event buffer '{}' is over its capacity of {} events: a reader "
                            "fell behind and missed {} event(s)",
                            m_Name, m_Capacity, keepFrom - oldestUnread);
        }
    } else {
        m_Overflowing = false;
    }

    const auto letGo = static_cast<std::size_t>(keepFrom - m_FirstKept);
    m_FirstKept = keepFrom;
    return letGo;
}

} // namespace Runic::Events::Detail
