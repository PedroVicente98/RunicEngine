// RenderFrameExchange (RenderFrameExchange.hpp): three slots and one atomic byte. The writer and
// the reader each own one slot; the byte holds the third slot's index and whether it carries a
// frame the reader has not taken. Publish swaps the writer's slot into the byte; ReadNewest swaps
// the reader's slot in when the byte is fresh.

#include "RenderData/RenderFrameExchange.hpp"

namespace Runic::RenderData {

namespace {

constexpr std::uint8_t kFirstWritingSlot = 0;
constexpr std::uint8_t kFirstNewestSlot = 1;
constexpr std::uint8_t kFirstReadingSlot = 2;

} // namespace

RenderFrameExchange::RenderFrameExchange(const RenderFrameCapacity &capacity)
    : m_Slots{RenderFrame(capacity), RenderFrame(capacity), RenderFrame(capacity)},
      m_Newest(kFirstNewestSlot), m_Writing(kFirstWritingSlot), m_Reading(kFirstReadingSlot) {}

RenderFrameExchange::RenderFrameExchange(const DebugGeometryCapacity &capacity)
    : RenderFrameExchange(RenderFrameCapacity{capacity, kDefaultShapeInstanceCapacity, {}}) {}

RenderFrame &RenderFrameExchange::WriteFrame() noexcept { return m_Slots[m_Writing]; }

void RenderFrameExchange::Publish() noexcept {
    const std::uint64_t number = m_Published.load(std::memory_order_relaxed) + 1;
    m_Slots[m_Writing].number = number;
    const std::uint8_t previous = m_Newest.exchange(
        static_cast<std::uint8_t>(m_Writing | kFreshBit), std::memory_order_acq_rel);
    m_Writing = previous & kSlotMask;
    m_Published.store(number, std::memory_order_release);
}

const RenderFrame *RenderFrameExchange::ReadNewest() noexcept {
    if ((m_Newest.load(std::memory_order_acquire) & kFreshBit) != 0) {
        const std::uint8_t previous = m_Newest.exchange(m_Reading, std::memory_order_acq_rel);
        m_Reading = previous & kSlotMask;
        m_ReaderHasFrame = true;
    }
    return m_ReaderHasFrame ? &m_Slots[m_Reading] : nullptr;
}

std::uint64_t RenderFrameExchange::PublishedFrames() const noexcept {
    return m_Published.load(std::memory_order_acquire);
}

} // namespace Runic::RenderData
