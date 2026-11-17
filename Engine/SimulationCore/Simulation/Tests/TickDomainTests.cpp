#include "TickDomain.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <type_traits>

using namespace Runic;
using namespace Runic::Simulation;
using namespace std::chrono_literals;

// "An untagged 9 ticks is a bug": a count is never made from a bare number.
static_assert(!std::is_constructible_v<DomainTickCount, std::uint64_t>);
static_assert(!std::is_convertible_v<std::uint64_t, DomainTickCount>);
static_assert(!std::is_default_constructible_v<DomainTickCount>);

TEST(TickDomain, TheDomainsAreSixtyThirtyTwentyAndTenHertz) {
    EXPECT_EQ(TicksPerSecond(TickDomain::Hz60), 60u);
    EXPECT_EQ(TicksPerSecond(TickDomain::Hz30), 30u);
    EXPECT_EQ(TicksPerSecond(TickDomain::Hz20), 20u);
    EXPECT_EQ(TicksPerSecond(TickDomain::Hz10), 10u);
    EXPECT_EQ(MasterTicksPerTick(TickDomain::Hz60), 1u);
    EXPECT_EQ(MasterTicksPerTick(TickDomain::Hz30), 2u);
    EXPECT_EQ(MasterTicksPerTick(TickDomain::Hz20), 3u);
    EXPECT_EQ(MasterTicksPerTick(TickDomain::Hz10), 6u);
    EXPECT_EQ(RateOf(TickDomain::Hz20), Core::TickRate::Hz(20));
    EXPECT_FLOAT_EQ(StepSeconds(TickDomain::Hz60), 1.0f / 60.0f);
    EXPECT_FLOAT_EQ(StepSeconds(TickDomain::Hz30), 1.0f / 30.0f);
    EXPECT_FLOAT_EQ(StepSeconds(TickDomain::Hz10), 0.1f);
    EXPECT_EQ(TickDomainName(TickDomain::Hz60), "hz60");
    EXPECT_EQ(TickDomainName(TickDomain::Hz30), "hz30");
    EXPECT_EQ(TickDomainName(TickDomain::Hz20), "hz20");
    EXPECT_EQ(TickDomainName(TickDomain::Hz10), "hz10");
    EXPECT_EQ(kTickDomains.size(), kTickDomainCount);
}

TEST(TickDomain, ADomainIsDueOnEveryMasterTickItsRateDivides) {
    for (std::uint64_t master = 0; master < 120; ++master) {
        const Core::TickId tick(master);
        EXPECT_TRUE(IsDue(TickDomain::Hz60, tick));
        EXPECT_EQ(IsDue(TickDomain::Hz30, tick), master % 2 == 0) << master;
        EXPECT_EQ(IsDue(TickDomain::Hz20, tick), master % 3 == 0) << master;
        EXPECT_EQ(IsDue(TickDomain::Hz10, tick), master % 6 == 0) << master;
    }
    EXPECT_EQ(DomainTickAt(TickDomain::Hz30, Core::TickId(90)), 45u);
    EXPECT_EQ(DomainTickAt(TickDomain::Hz20, Core::TickId(90)), 30u);
    EXPECT_EQ(DomainTickAt(TickDomain::Hz10, Core::TickId(90)), 15u);
}

TEST(TickDomain, ADomainTickHappensAtTheSameTimeAsItsMasterTick) {
    // The time of a tick comes from its number in its own domain, and that is the master
    // tick's time to the nanosecond: the domains never drift apart.
    for (std::uint64_t master = 0; master < 100'000; master += 6) {
        const Core::TickId tick(master);
        const Core::Duration time = RateOf(TickDomain::Hz60).TimeOfTick(master);
        for (const TickDomain domain : kTickDomains) {
            ASSERT_EQ(RateOf(domain).TimeOfTick(DomainTickAt(domain, tick)), time)
                << TickDomainName(domain) << " at master tick " << master;
        }
    }
}

TEST(DomainTickCount, LastsExactlyItsTicks) {
    const DomainTickCount recovery(TickDomain::Hz30, 45);
    EXPECT_EQ(recovery.Domain(), TickDomain::Hz30);
    EXPECT_EQ(recovery.Ticks(), 45u);
    EXPECT_EQ(recovery.ToDuration(), Core::Duration(1500ms));
    EXPECT_EQ(DomainTickCount(TickDomain::Hz10, 36'000).ToDuration(), Core::Duration(1h));
    EXPECT_EQ(DomainTickCount(TickDomain::Hz20, 0).ToDuration(), Core::Duration(0));
}

TEST(DomainTickCount, CoversADurationAndIsNeverShorter) {
    EXPECT_EQ(DomainTickCount::Covering(TickDomain::Hz30, 1500ms).Ticks(), 45u);
    EXPECT_EQ(DomainTickCount::Covering(TickDomain::Hz30, 1501ms).Ticks(), 46u); // rounds up
    EXPECT_EQ(DomainTickCount::Covering(TickDomain::Hz10, 1ms).Ticks(), 1u);
    EXPECT_EQ(DomainTickCount::Covering(TickDomain::Hz60, 0ms).Ticks(), 0u);
    EXPECT_EQ(DomainTickCount::Covering(TickDomain::Hz20, 24h).Ticks(), 1'728'000u);

    for (const TickDomain domain : kTickDomains) {
        for (Core::Duration duration = 0ms; duration < 2s; duration += 7ms) {
            const DomainTickCount count = DomainTickCount::Covering(domain, duration);
            ASSERT_GE(count.ToDuration(), duration) << TickDomainName(domain);
            if (count.Ticks() > 0) {
                ASSERT_LT(DomainTickCount(domain, count.Ticks() - 1).ToDuration(), duration)
                    << TickDomainName(domain); // and one tick fewer would be too short
            }
        }
    }
}

TEST(DomainTickCount, ConvertsToAnotherDomainWithoutGettingShorter) {
    EXPECT_EQ(DomainTickCount(TickDomain::Hz30, 45).In(TickDomain::Hz10).Ticks(), 15u);
    EXPECT_EQ(DomainTickCount(TickDomain::Hz30, 46).In(TickDomain::Hz10).Ticks(), 16u); // up
    EXPECT_EQ(DomainTickCount(TickDomain::Hz10, 15).In(TickDomain::Hz30).Ticks(), 45u);
    EXPECT_EQ(DomainTickCount(TickDomain::Hz20, 7).In(TickDomain::Hz60).Ticks(), 21u);
    EXPECT_EQ(DomainTickCount(TickDomain::Hz20, 7).In(TickDomain::Hz20).Ticks(), 7u);
    EXPECT_EQ(DomainTickCount(TickDomain::Hz20, 7).In(TickDomain::Hz30).Domain(), TickDomain::Hz30);

    for (const TickDomain from : kTickDomains) {
        for (const TickDomain to : kTickDomains) {
            for (std::uint64_t ticks = 0; ticks < 200; ++ticks) {
                const DomainTickCount count(from, ticks);
                ASSERT_GE(count.In(to).ToDuration(), count.ToDuration());
            }
        }
    }
}

TEST(DomainTickCount, AddsSubtractsAndComparesInsideOneDomain) {
    const DomainTickCount a(TickDomain::Hz30, 30);
    const DomainTickCount b(TickDomain::Hz30, 12);
    EXPECT_EQ((a + b).Ticks(), 42u);
    EXPECT_EQ((a - b).Ticks(), 18u);
    EXPECT_TRUE(b < a);
    EXPECT_TRUE(a >= b);
    EXPECT_TRUE(a == DomainTickCount(TickDomain::Hz30, 30));
    EXPECT_TRUE(a != b);
}

#if RUNIC_ASSERTS_ENABLED

TEST(DomainTickCountDeathTest, CountsOfTwoDomainsDoNotMix) {
    const DomainTickCount realtime(TickDomain::Hz30, 9);
    const DomainTickCount slow(TickDomain::Hz10, 9);
    EXPECT_DEATH(static_cast<void>(realtime + slow), "two domains were mixed");
    EXPECT_DEATH(static_cast<void>(realtime == slow), "two domains were mixed");
    EXPECT_DEATH(static_cast<void>(realtime < slow), "two domains were mixed");
}

#endif // RUNIC_ASSERTS_ENABLED
