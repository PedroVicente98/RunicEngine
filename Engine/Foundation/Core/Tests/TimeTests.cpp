#include "Time.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>

using namespace Runic::Core;
using namespace std::chrono_literals;

TEST(Time, SecondsConvertBothWays) {
    EXPECT_EQ(FromSeconds(1.5), Duration(1'500'000'000));
    EXPECT_EQ(FromSeconds(0.016), Duration(16'000'000));
    EXPECT_EQ(FromSeconds(-0.25), Duration(-250'000'000));
    EXPECT_EQ(FromSeconds(1.0000000004),
              Duration(1'000'000'000)); // under half a nanosecond rounds down
    EXPECT_EQ(FromSeconds(1.0000000006),
              Duration(1'000'000'001)); // over half a nanosecond rounds up
    EXPECT_DOUBLE_EQ(ToSeconds(Duration(2'500'000'000)), 2.5);
    EXPECT_DOUBLE_EQ(ToSeconds(FromSeconds(0.125)), 0.125);
}

TEST(Time, SixtyHertzTicksAreExact) {
    const TickRate rate = TickRate::Hz(60);
    EXPECT_EQ(rate.TicksPerSecond(), 60u);
    EXPECT_EQ(rate.TimeOfTick(0), Duration(0));
    EXPECT_EQ(rate.TimeOfTick(1), Duration(16'666'667));
    EXPECT_EQ(rate.TimeOfTick(3), Duration(50ms));
    EXPECT_EQ(rate.TimeOfTick(60), Duration(1s));
    EXPECT_EQ(rate.TimeOfTick(216'000'000),
              Duration(std::chrono::hours(1000))); // 1,000 hours: no drift
}

TEST(Time, TicksInIsTheInverseOfTimeOfTick) {
    for (const std::uint32_t hz : {60u, 30u, 20u, 10u}) {
        const TickRate rate = TickRate::Hz(hz);
        for (std::uint64_t tick = 0; tick < 10'000; ++tick) {
            ASSERT_EQ(rate.TicksIn(rate.TimeOfTick(tick)), tick) << hz << " Hz, tick " << tick;
            if (tick > 0) {
                ASSERT_EQ(rate.TicksIn(rate.TimeOfTick(tick) - Duration(1)), tick - 1)
                    << hz << " Hz, tick " << tick;
            }
        }
        const std::uint64_t far = 10'000'000'000; // 5 years at 60 Hz, 31 years at 10 Hz
        EXPECT_EQ(rate.TicksIn(rate.TimeOfTick(far)), far) << hz << " Hz";
    }
}

TEST(Time, DomainsShareTheMasterClock) {
    // Every 30 Hz tick is exactly two 60 Hz ticks, every 10 Hz tick six (RunicDocs › Simulation
    // core clock).
    const TickRate master = TickRate::Hz(60);
    for (std::uint64_t tick = 0; tick < 1'000; ++tick) {
        ASSERT_EQ(TickRate::Hz(30).TimeOfTick(tick), master.TimeOfTick(2 * tick));
        ASSERT_EQ(TickRate::Hz(20).TimeOfTick(tick), master.TimeOfTick(3 * tick));
        ASSERT_EQ(TickRate::Hz(10).TimeOfTick(tick), master.TimeOfTick(6 * tick));
    }
}

TEST(Time, RatesCompareByTicksPerSecond) {
    EXPECT_EQ(TickRate::Hz(30), TickRate::Hz(30));
    EXPECT_NE(TickRate::Hz(30), TickRate::Hz(20));
}

TEST(Time, MonotonicNowNeverGoesBack) {
    const TimePoint first = MonotonicNow();
    const TimePoint second = MonotonicNow();
    EXPECT_LE(first, second);
}
