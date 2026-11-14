#pragma once

#include <chrono>
#include <cstdint>

#include "Assert.hpp"

// Time: one monotonic clock and integer nanoseconds. Time is never accumulated in floating point:
// durations add as integers, and a fixed rate computes the time of tick k from k (RunicDocs ›
// Application and LayerStack › Implementation rules; Simulation core clock and threads).

namespace Runic::Core {

using Duration = std::chrono::nanoseconds; // signed 64-bit nanoseconds: about ±292 years
using MonotonicClock = std::chrono::steady_clock;
using TimePoint = std::chrono::time_point<MonotonicClock, Duration>;

// The process's one monotonic clock. It never goes back and says nothing about the time of day.
TimePoint MonotonicNow() noexcept;

// For display and for APIs that take seconds; never accumulate the result.
double ToSeconds(Duration duration) noexcept;

// Rounded to the nearest nanosecond. seconds must be finite and within Duration's range.
Duration FromSeconds(double seconds) noexcept;

// A fixed rate in whole ticks per second (the 60 Hz master clock, the 30, 20 and 10 Hz domains).
// Tick 0 happens at time zero and tick k at TimeOfTick(k), computed from k with exact integer
// arithmetic, so a run of any length never drifts. Valid while the times fit in Duration.
class TickRate {
  public:
    static constexpr TickRate Hz(std::uint32_t ticksPerSecond) noexcept {
        RUNIC_ASSERT(ticksPerSecond > 0);
        return TickRate(ticksPerSecond);
    }

    constexpr std::uint32_t TicksPerSecond() const noexcept { return m_TicksPerSecond; }

    // When tick k happens, after tick 0: ⌈k × 10⁹ / rate⌉ nanoseconds, the first whole nanosecond
    // at or after the exact instant. At 60 Hz tick 1 is at 16 666 667 ns and tick 60 at exactly 1
    // s.
    Duration TimeOfTick(std::uint64_t tick) const noexcept;

    // How many whole ticks have passed after elapsed (not negative): ⌊elapsed × rate / 10⁹⌋. It is
    // the exact inverse of TimeOfTick: TicksIn(TimeOfTick(k)) == k, and TicksIn(TimeOfTick(k) - 1
    // ns) == k - 1.
    std::uint64_t TicksIn(Duration elapsed) const noexcept;

    friend constexpr bool operator==(const TickRate &, const TickRate &) noexcept = default;

  private:
    constexpr explicit TickRate(std::uint32_t ticksPerSecond) noexcept
        : m_TicksPerSecond(ticksPerSecond) {}

    std::uint32_t m_TicksPerSecond;
};

} // namespace Runic::Core
