#include "AllocationCounter.hpp"
#include "FixedRingBuffer.hpp"
#include "FixedVector.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <utility>

using namespace Runic::Core;

// A named namespace (not an anonymous one), so the containers' members instantiated for these types
// have external linkage.
namespace FixedContainersTesting {

// Counts live instances, to check that a container constructs and destroys exactly what it holds.
struct Tracked {
    static inline int s_Live = 0;
    int value = 0;

    explicit Tracked(int v) : value(v) { ++s_Live; }
    Tracked(const Tracked &other) : value(other.value) { ++s_Live; }
    Tracked(Tracked &&other) noexcept : value(other.value) { ++s_Live; }
    Tracked &operator=(const Tracked &) = default;
    Tracked &operator=(Tracked &&) noexcept = default;
    ~Tracked() { --s_Live; }
};

// A small plain event, like the application events APP-1 queues.
struct SampleEvent {
    int kind = 0;
    float x = 0.0f;
    float y = 0.0f;
};

} // namespace FixedContainersTesting

using FixedContainersTesting::SampleEvent;
using FixedContainersTesting::Tracked;

TEST(FixedContainers, RingBufferIsFirstInFirstOut) {
    FixedRingBuffer<int, 4> queue;
    EXPECT_TRUE(queue.IsEmpty());
    EXPECT_EQ(queue.Capacity(), 4u);
    for (int i = 1; i <= 4; ++i) {
        EXPECT_TRUE(queue.TryPush(i));
    }
    EXPECT_TRUE(queue.IsFull());
    EXPECT_EQ(queue.Front(), 1);
    int out = 0;
    for (int i = 1; i <= 4; ++i) {
        ASSERT_TRUE(queue.TryPop(out));
        EXPECT_EQ(out, i);
    }
    EXPECT_FALSE(queue.TryPop(out));
}

TEST(FixedContainers, RingBufferWrapsAround) {
    FixedRingBuffer<int, 3> queue;
    int next = 0;
    int expected = 0;
    for (int round = 0; round < 100; ++round) {
        while (queue.TryPush(next)) {
            ++next;
        }
        queue.Pop();
        ++expected;
        ASSERT_EQ(queue.Front(), expected);
        ASSERT_EQ(queue.Size(), 2u);
    }
}

TEST(FixedContainers, FullRingBufferRefusesAndKeepsItsContents) {
    FixedRingBuffer<int, 2> queue;
    EXPECT_TRUE(queue.TryPush(10));
    EXPECT_TRUE(queue.TryPush(20));
    EXPECT_FALSE(queue.TryPush(30));
    EXPECT_EQ(queue.Size(), 2u);
    EXPECT_EQ(queue.Front(), 10);
}

TEST(FixedContainers, RingBufferDestroysWhatItHolds) {
    {
        FixedRingBuffer<Tracked, 8> queue;
        for (int i = 0; i < 5; ++i) {
            ASSERT_TRUE(queue.TryPush(Tracked(i)));
        }
        EXPECT_EQ(Tracked::s_Live, 5);
        queue.Pop();
        EXPECT_EQ(Tracked::s_Live, 4);
        queue.Clear();
        EXPECT_EQ(Tracked::s_Live, 0);
        ASSERT_TRUE(queue.TryPush(Tracked(9)));
    }
    EXPECT_EQ(Tracked::s_Live, 0); // the destructor destroyed the last one
}

TEST(FixedContainers, VectorPushPopAndIndex) {
    FixedVector<int, 4> list;
    EXPECT_TRUE(list.IsEmpty());
    for (int i = 0; i < 4; ++i) {
        EXPECT_TRUE(list.TryPushBack(i * 10));
    }
    EXPECT_FALSE(list.TryPushBack(99));
    EXPECT_TRUE(list.IsFull());
    EXPECT_EQ(list[2], 20);
    int sum = 0;
    for (const int value : list) {
        sum += value;
    }
    EXPECT_EQ(sum, 60);
    list.PopBack();
    EXPECT_EQ(list.Size(), 3u);
    list.Clear();
    EXPECT_TRUE(list.IsEmpty());
}

TEST(FixedContainers, VectorDestroysWhatItHolds) {
    {
        FixedVector<Tracked, 4> list;
        ASSERT_TRUE(list.TryPushBack(Tracked(1)));
        ASSERT_TRUE(list.TryPushBack(Tracked(2)));
        EXPECT_EQ(Tracked::s_Live, 2);
        list.PopBack();
        EXPECT_EQ(Tracked::s_Live, 1);
    }
    EXPECT_EQ(Tracked::s_Live, 0);
}

TEST(FixedContainers, NeverAllocateAfterConstruction) {
    FixedRingBuffer<SampleEvent, 256> queue;
    FixedVector<Tracked, 64> list;
    bool everyOperationWorked = true;
    std::size_t allocations = 0;
    {
        Testing::AllocationScope scope;
        for (int i = 0; i < 10'000; ++i) {
            SampleEvent event;
            if (!queue.TryPush(SampleEvent{i, 1.0f, 2.0f}) || !queue.TryPop(event) ||
                event.kind != i) {
                everyOperationWorked = false;
            }
            if (!list.TryPushBack(Tracked(i))) {
                everyOperationWorked = false;
            }
            list.PopBack();
        }
        allocations = scope.Count();
    }
    EXPECT_TRUE(everyOperationWorked);
    EXPECT_EQ(allocations, 0u);
}

#if RUNIC_ASSERTS_ENABLED
TEST(FixedContainersDeathTest, FrontOfAnEmptyRingBufferAsserts) {
    FixedRingBuffer<int, 2> queue;
    EXPECT_DEATH(static_cast<void>(queue.Front()), "assert failed");
}

TEST(FixedContainersDeathTest, IndexPastTheEndAsserts) {
    FixedVector<int, 2> list;
    ASSERT_TRUE(list.TryPushBack(1));
    EXPECT_DEATH(static_cast<void>(list[1]), "assert failed");
}
#endif
