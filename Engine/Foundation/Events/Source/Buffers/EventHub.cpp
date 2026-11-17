// The EventHub (EventHub.hpp) and the part of a buffer that does not depend on its event type.

#include "EventHub.hpp"

#include <algorithm>

#include "Assert.hpp"

namespace Runic::Events {

EventHub::EventHub(Core::SimulationId simulation, std::uint64_t nextSequence)
    : m_Sequence(simulation, nextSequence) {}

EventHub::~EventHub() {
    RUNIC_ASSERT_MSG(m_Buffers.empty(), "an EventHub was destroyed before its buffers");
}

std::size_t EventHub::Flush() {
    std::size_t flushed = 0;
    for (Detail::EventBufferBase *buffer : m_Buffers) {
        flushed += buffer->Flush();
    }
    return flushed;
}

std::uint32_t EventHub::BufferCount() const noexcept {
    return static_cast<std::uint32_t>(m_Buffers.size());
}

EventBufferInfo EventHub::Describe(std::uint32_t index) const {
    RUNIC_ASSERT(index < m_Buffers.size());
    return m_Buffers[index]->Describe();
}

namespace Detail {

EventBufferBase::EventBufferBase(EventHub &hub, std::string_view name, std::size_t capacity)
    : m_Hub(hub), m_Name(name), m_Capacity(capacity) {
    RUNIC_ASSERT(!name.empty());
    RUNIC_ASSERT(capacity > 0);
    m_Hub.m_Buffers.push_back(this);
}

EventBufferBase::~EventBufferBase() { std::erase(m_Hub.m_Buffers, this); }

Core::SimulationId EventBufferBase::Simulation() const noexcept { return m_Hub.Simulation(); }

EventSequence &EventBufferBase::Sequence() noexcept { return m_Hub.m_Sequence; }

EventBufferInfo EventBufferBase::Describe() const noexcept {
    EventBufferInfo info;
    info.name = m_Name;
    info.pending = m_Pending;
    info.readable = static_cast<std::size_t>(m_End - m_FirstKept);
    info.readers = static_cast<std::uint32_t>(std::count_if(
        m_Readers.begin(), m_Readers.end(), [](const Reader &reader) { return reader.active; }));
    info.flushed = m_End;
    info.dropped = m_Dropped;
    return info;
}

} // namespace Detail

} // namespace Runic::Events
