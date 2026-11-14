#include "PlainIds.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <map>
#include <type_traits>
#include <unordered_map>

using namespace Runic::Core;

// Two kinds never convert into each other, and an integer becomes an ID only explicitly (CORE-1:
// unit tests cover IDs).
static_assert(!std::is_convertible_v<GlobalEntityId, GlobalPlayerId>);
static_assert(!std::is_constructible_v<GlobalPlayerId, GlobalEntityId>);
static_assert(!std::is_convertible_v<WorldId, InstanceId>);
static_assert(!std::is_convertible_v<std::uint64_t, GlobalEntityId>);
static_assert(std::is_constructible_v<GlobalEntityId, std::uint64_t>);
static_assert(!std::is_convertible_v<GlobalEntityId, std::uint64_t>);
static_assert(!std::is_convertible_v<std::uint64_t, TickId>);
static_assert(!std::is_convertible_v<TickId, std::uint64_t>);

// The widths of the plan, and plain-value behavior.
static_assert(sizeof(GlobalEntityId) == 8 && sizeof(GlobalPlayerId) == 8 && sizeof(AssetId) == 8);
static_assert(sizeof(SimulationId) == 4 && sizeof(WorldId) == 4 && sizeof(InstanceId) == 4);
static_assert(sizeof(WaterBodyId) == 4 && sizeof(FieldId) == 4 && sizeof(WidgetId) == 4);
static_assert(sizeof(TickId) == 8 && sizeof(AuthorityEpoch) == 4);
static_assert(std::is_trivially_copyable_v<GlobalEntityId> && std::is_trivially_copyable_v<TickId>);
static_assert(!GlobalEntityId().IsValid() && GlobalEntityId(7).IsValid());

TEST(PlainIds, DefaultIsInvalid) {
    EXPECT_FALSE(GlobalEntityId().IsValid());
    EXPECT_EQ(GlobalEntityId(), GlobalEntityId::Invalid());
    EXPECT_EQ(WorldId().Value(), 0u);
    EXPECT_TRUE(WorldId(3).IsValid());
    EXPECT_EQ(WorldId(3).Value(), 3u);
}

TEST(PlainIds, CompareAndHashByValue) {
    EXPECT_EQ(GlobalPlayerId(42), GlobalPlayerId(42));
    EXPECT_NE(GlobalPlayerId(42), GlobalPlayerId(43));
    EXPECT_LT(GlobalPlayerId(42), GlobalPlayerId(43));

    std::map<SimulationId, int> ordered{{SimulationId(2), 20}, {SimulationId(1), 10}};
    EXPECT_EQ(ordered.begin()->first, SimulationId(1));

    std::unordered_map<GlobalEntityId, int> hashed;
    hashed[GlobalEntityId(1u << 20)] = 5;
    hashed[GlobalEntityId(9)] = 6;
    EXPECT_EQ(hashed.at(GlobalEntityId(1u << 20)), 5);
    EXPECT_EQ(hashed.count(GlobalEntityId(8)), 0u);
}

TEST(PlainIds, TickIdCountsForward) {
    EXPECT_EQ(TickId().Value(), 0u);
    EXPECT_EQ(TickId().Next(), TickId(1));
    EXPECT_EQ(TickId(10) + 5, TickId(15));
    EXPECT_EQ(TickId(15) - TickId(10), 5u);
    EXPECT_LT(TickId(9), TickId(10));
}

TEST(PlainIds, AuthorityEpochRises) {
    const AuthorityEpoch first;
    EXPECT_EQ(first.Value(), 0u);
    EXPECT_LT(first, first.Next());
    EXPECT_EQ(first.Next().Next(), AuthorityEpoch(2));
}

#if RUNIC_ASSERTS_ENABLED
TEST(PlainIdsDeathTest, TickDifferenceBackwardsAsserts) {
    EXPECT_DEATH(static_cast<void>(TickId(1) - TickId(2)), "assert failed");
}
#endif
