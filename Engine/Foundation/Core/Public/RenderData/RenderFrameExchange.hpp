#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>

#include "RenderData/RenderFrame.hpp"

// How frames move from the thread that writes them (extraction) to the thread that draws them
// (GodotBridge) without either ever waiting (RunicDocs › RenderData in Core › Double-buffered;
// slice 2 decision D1). The writer fills one frame while the reader holds another, and a third slot
// carries the newest published frame between them: publishing and reading are one atomic exchange
// each, never a lock. A reader that falls behind skips to the newest frame; a reader that is ahead
// keeps the frame it holds.

namespace Runic::RenderData {

class RenderFrameExchange {
  public:
    explicit RenderFrameExchange(const RenderFrameCapacity &capacity = {});
    explicit RenderFrameExchange(const DebugGeometryCapacity &capacity);
    RenderFrameExchange(const RenderFrameExchange &) = delete;
    RenderFrameExchange &operator=(const RenderFrameExchange &) = delete;

    // The writer's side: call from one thread only.
    // The frame to fill. It still holds what was written into it three frames ago: clear what you
    // rewrite.
    RenderFrame &WriteFrame() noexcept;
    // Stamps the frame's number and makes it the newest; WriteFrame then returns another frame.
    void Publish() noexcept;

    // The reader's side: call from one thread only (the writer's, or another one).
    // The newest published frame, held by the reader until its next call; the frame it already held
    // when nothing new was published; nullptr before the first Publish.
    const RenderFrame *ReadNewest() noexcept;

    // Frames published so far. Thread-safe.
    std::uint64_t PublishedFrames() const noexcept;

  private:
    static constexpr std::size_t kSlotCount = 3;
    static constexpr std::uint8_t kSlotMask = 0x03;
    // Set in m_Newest while its slot holds a frame the reader has not taken.
    static constexpr std::uint8_t kFreshBit = 0x04;

    std::array<RenderFrame, kSlotCount> m_Slots;
    std::atomic<std::uint8_t> m_Newest; // the slot between writer and reader, with kFreshBit
    std::atomic<std::uint64_t> m_Published{0};
    std::uint8_t m_Writing; // writer only
    std::uint8_t m_Reading; // reader only
    bool m_ReaderHasFrame = false;
};

static_assert(std::atomic<std::uint8_t>::is_always_lock_free, "the frame exchange must never lock");

} // namespace Runic::RenderData
