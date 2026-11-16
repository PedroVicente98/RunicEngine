#include "TickArena.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

#include "AllocationCounter.hpp"
#include "PerWorker.hpp"

using namespace Runic::Core;

namespace {

struct Sample {
    std::uint64_t id;
    float weight;
};

// One tick's worth of allocations of mixed sizes and alignments.
std::size_t FillOneTick(TickArena &arena) {
    std::size_t bytes = 0;
    for (std::size_t round = 0; round < 200; ++round) {
        std::memset(arena.Allocate(24 + round % 40, alignof(std::max_align_t)), 0xAB,
                    24 + round % 40);
        bytes += 24 + round % 40;
        Sample *samples = arena.AllocateArray<Sample>(8);
        samples[7] = Sample{round, 1.0f};
        bytes += 8 * sizeof(Sample);
    }
    return bytes;
}

} // namespace

TEST(TickArena, TakesNoMemoryUntilTheFirstAllocation) {
    std::size_t allocations = 0;
    {
        Testing::AllocationScope scope;
        const TickArena arena;
        allocations = scope.Count();
        EXPECT_EQ(arena.ReservedBytes(), 0u);
        EXPECT_EQ(arena.BlockCount(), 0u);
    }
    EXPECT_EQ(allocations, 0u);
}

TEST(TickArena, AllocationsAreAlignedAndDoNotOverlap) {
    TickArena arena(1024);
    struct Piece {
        std::byte *start;
        std::size_t bytes;
    };
    std::vector<Piece> pieces;
    const std::size_t alignments[] = {1, 2, 4, 8, 16, 32, 64};
    for (std::size_t round = 0; round < 300; ++round) {
        const std::size_t alignment = alignments[round % std::size(alignments)];
        const std::size_t bytes = 1 + round % 37;
        auto *start = static_cast<std::byte *>(arena.Allocate(bytes, alignment));
        ASSERT_NE(start, nullptr);
        ASSERT_EQ(reinterpret_cast<std::uintptr_t>(start) % alignment, 0u) << "round " << round;
        std::memset(start, static_cast<int>(round % 251), bytes);
        pieces.push_back({start, bytes});
    }
    // Every piece still holds what was written to it: none overlaps another.
    for (std::size_t round = 0; round < pieces.size(); ++round) {
        for (std::size_t byte = 0; byte < pieces[round].bytes; ++byte) {
            ASSERT_EQ(pieces[round].start[byte], static_cast<std::byte>(round % 251))
                << "round " << round;
        }
    }
    EXPECT_GT(arena.BlockCount(), 1u);
    EXPECT_GE(arena.ReservedBytes(), arena.UsedBytes());
}

TEST(TickArena, ResetKeepsItsMemoryAndTheNextTickAllocatesNothing) {
    TickArena arena(4 * 1024);
    const std::size_t asked = FillOneTick(arena); // the first tick takes the blocks
    const std::size_t reserved = arena.ReservedBytes();
    const std::size_t blocks = arena.BlockCount();
    EXPECT_GE(arena.UsedBytes(), asked);
    EXPECT_GT(blocks, 1u);

    std::size_t allocations = 0;
    {
        Testing::AllocationScope scope;
        for (int tick = 0; tick < 100; ++tick) {
            arena.Reset();
            FillOneTick(arena);
        }
        allocations = scope.Count();
    }

    EXPECT_EQ(allocations, 0u);                 // nothing taken from the system again
    EXPECT_EQ(arena.ReservedBytes(), reserved); // and nothing returned to it
    EXPECT_EQ(arena.BlockCount(), blocks);
}

TEST(TickArena, ResetGivesTheSameStorageAgain) {
    TickArena arena;
    void *first = arena.Allocate(100, 8);
    void *second = arena.Allocate(100, 8);
    EXPECT_NE(first, second);
    EXPECT_GE(arena.UsedBytes(), 200u);

    arena.Reset();
    EXPECT_EQ(arena.UsedBytes(), 0u);
    EXPECT_EQ(arena.Allocate(100, 8), first);
    EXPECT_EQ(arena.Allocate(100, 8), second);
}

TEST(TickArena, ARequestLargerThanABlockGetsItsOwnBlockAndKeepsIt) {
    TickArena arena(256);
    void *small = arena.Allocate(64, 8);
    void *large = arena.Allocate(10'000, 64);
    ASSERT_NE(large, nullptr);
    EXPECT_EQ(reinterpret_cast<std::uintptr_t>(large) % 64, 0u);
    std::memset(large, 0x5A, 10'000);
    EXPECT_EQ(arena.BlockCount(), 2u);
    EXPECT_GE(arena.ReservedBytes(), 10'256u);

    arena.Reset();
    std::size_t allocations = 0;
    {
        Testing::AllocationScope scope;
        EXPECT_EQ(arena.Allocate(64, 8), small);
        EXPECT_EQ(arena.Allocate(10'000, 64), large);
        allocations = scope.Count();
    }
    EXPECT_EQ(allocations, 0u);
    EXPECT_EQ(arena.BlockCount(), 2u);
}

TEST(TickArena, CreateBuildsAValueInTheArena) {
    TickArena arena;
    Sample *sample = arena.Create<Sample>(Sample{42, 0.5f});
    ASSERT_NE(sample, nullptr);
    EXPECT_EQ(sample->id, 42u);
    EXPECT_FLOAT_EQ(sample->weight, 0.5f);
    EXPECT_EQ(reinterpret_cast<std::uintptr_t>(sample) % alignof(Sample), 0u);
}

TEST(TickArena, EachWorkerHasItsOwnArena) {
    PerWorker<TickArena> arenas(3);
    void *a = arenas[0].Allocate(32, 8);
    void *b = arenas[1].Allocate(32, 8);
    EXPECT_NE(a, b);
    EXPECT_GT(arenas[0].UsedBytes(), 0u);
    EXPECT_EQ(arenas[2].UsedBytes(), 0u);
    arenas[0].Reset();
    EXPECT_EQ(arenas[0].UsedBytes(), 0u);
    EXPECT_GT(arenas[1].UsedBytes(), 0u);
}
