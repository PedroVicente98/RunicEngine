// CORE-3 acceptance tests: a frame is written on one thread and read on another with no lock.

#include "AllocationCounter.hpp"
#include "RenderData/RenderFrameExchange.hpp"

#include <gtest/gtest.h>

#include <atomic>
#include <cstdint>
#include <thread>

using namespace Runic::RenderData;
using Runic::Core::Vec3;
using Runic::Core::WorldPosition;
namespace Testing = Runic::Core::Testing;

namespace {

constexpr DebugGeometryCapacity kSmallCapacity{
    .lineVertices = 64, .triangleVertices = 64, .texts = 4, .textBytes = 64};
constexpr std::uint64_t kThreadedFrames = 20'000;
constexpr std::uint64_t kRounds = 12;
constexpr std::uint32_t kLineCountCycle = 8; // frame n holds n % kLineCountCycle + 1 lines

// Fills frame n so that every part of it says n: a reader that finds two numbers in one frame read
// a torn frame.
void WriteNumbered(RenderFrame &frame, std::uint64_t n) {
    frame.debug.Clear();
    const auto value = static_cast<float>(n);
    frame.origin = WorldPosition{static_cast<double>(n), 0.0, 0.0};
    frame.camera.position = Vec3(value);
    const auto lines = static_cast<std::uint32_t>(n % kLineCountCycle) + 1;
    for (std::uint32_t line = 0; line < lines; ++line) {
        frame.debug.AddLine(DebugBatch::WorldDepth, Vec3(value), Vec3(value),
                            static_cast<PackedColor>(n));
    }
}

bool IsWhole(const RenderFrame &frame) {
    const std::uint64_t n = frame.number;
    const auto value = static_cast<float>(n);
    if (frame.origin.x != static_cast<double>(n) || frame.camera.position.x != value) {
        return false;
    }
    const auto colors = frame.debug.Colors(DebugBatch::WorldDepth, DebugPrimitive::Lines);
    const auto expectedVertices =
        (static_cast<std::uint32_t>(n % kLineCountCycle) + 1) * kVerticesPerLine;
    if (colors.size() != expectedVertices) {
        return false;
    }
    for (const PackedColor color : colors) {
        if (color != static_cast<PackedColor>(n)) {
            return false;
        }
    }
    return true;
}

void PublishNumbered(RenderFrameExchange &frames) {
    WriteNumbered(frames.WriteFrame(), frames.PublishedFrames() + 1);
    frames.Publish();
}

} // namespace

TEST(RenderFrameExchange, NothingToReadBeforeTheFirstFrame) {
    RenderFrameExchange frames(kSmallCapacity);
    EXPECT_EQ(frames.ReadNewest(), nullptr);
    EXPECT_EQ(frames.PublishedFrames(), 0u);
}

TEST(RenderFrameExchange, TheReaderGetsTheNewestFrame) {
    RenderFrameExchange frames(kSmallCapacity);
    constexpr std::uint64_t kPublished = 3;
    for (std::uint64_t n = 1; n <= kPublished; ++n) {
        PublishNumbered(frames);
    }
    const RenderFrame *newest = frames.ReadNewest();
    ASSERT_NE(newest, nullptr);
    EXPECT_EQ(newest->number, kPublished);
    EXPECT_TRUE(IsWhole(*newest));
    EXPECT_EQ(frames.ReadNewest(), newest)
        << "nothing new was published: the reader keeps its frame";
    EXPECT_EQ(frames.PublishedFrames(), kPublished);
}

TEST(RenderFrameExchange, WriterAndReaderNeverShareAFrame) {
    RenderFrameExchange frames(kSmallCapacity);
    for (std::uint64_t round = 0; round < kRounds; ++round) {
        RenderFrame &written = frames.WriteFrame();
        PublishNumbered(frames);
        const RenderFrame *read = frames.ReadNewest();
        ASSERT_NE(read, nullptr);
        EXPECT_EQ(read, &written);
        EXPECT_NE(&frames.WriteFrame(), read) << "the writer's next frame is the one being read";
    }
}

TEST(RenderFrameExchange, AFrameWrittenOnOneThreadIsReadWholeOnAnother) {
    RenderFrameExchange frames(kSmallCapacity);
    std::atomic<bool> writerDone{false};
    std::thread writer([&frames, &writerDone] {
        for (std::uint64_t n = 1; n <= kThreadedFrames; ++n) {
            PublishNumbered(frames);
        }
        writerDone.store(true, std::memory_order_release);
    });
    std::uint64_t last = 0;
    std::uint64_t distinctFramesRead = 0;
    bool everyFrameWhole = true;
    bool neverBackwards = true;
    while (last != kThreadedFrames) {
        const bool done = writerDone.load(std::memory_order_acquire);
        if (const RenderFrame *frame = frames.ReadNewest(); frame != nullptr) {
            everyFrameWhole = everyFrameWhole && IsWhole(*frame);
            neverBackwards = neverBackwards && frame->number >= last;
            distinctFramesRead += frame->number != last ? 1 : 0;
            last = frame->number;
        }
        if (done && last != kThreadedFrames && frames.PublishedFrames() != kThreadedFrames) {
            break; // the writer ended without publishing every frame: the assertions below report
                   // it
        }
    }
    writer.join();
    EXPECT_TRUE(everyFrameWhole) << "a frame mixed two frames' contents";
    EXPECT_TRUE(neverBackwards);
    EXPECT_EQ(last, kThreadedFrames);
    EXPECT_GE(distinctFramesRead, 1u);
}

TEST(RenderFrameExchange, TheExchangeIsOneLockFreeAtomic) {
    // RenderFrameExchange.hpp also static_asserts it, so a platform without it does not build.
    EXPECT_TRUE(std::atomic<std::uint8_t>{}.is_lock_free());
}

TEST(RenderFrameExchange, PublishingAndReadingNeverAllocate) {
    RenderFrameExchange frames(kSmallCapacity);
    bool everyReadFound = true;
    std::size_t allocations = 0;
    {
        Testing::AllocationScope scope;
        for (std::uint64_t round = 0; round < kRounds; ++round) {
            PublishNumbered(frames);
            everyReadFound = everyReadFound && frames.ReadNewest() != nullptr;
        }
        allocations = scope.Count();
    }
    EXPECT_TRUE(everyReadFound);
    EXPECT_EQ(allocations, 0u);
}
