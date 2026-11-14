// Time (Time.hpp): the monotonic clock, seconds conversions and exact tick arithmetic.

#include "Time.hpp"

#include <limits>

namespace Runic::Core {

namespace {
constexpr std::uint64_t kNanosecondsPerSecond = 1'000'000'000;
} // namespace

TimePoint MonotonicNow() noexcept {
    return std::chrono::time_point_cast<Duration>(MonotonicClock::now());
}

double ToSeconds(Duration duration) noexcept {
    return static_cast<double>(duration.count()) / static_cast<double>(kNanosecondsPerSecond);
}

Duration FromSeconds(double seconds) noexcept {
    const double nanoseconds = seconds * static_cast<double>(kNanosecondsPerSecond);
    return Duration(
        static_cast<Duration::rep>(nanoseconds < 0.0 ? nanoseconds - 0.5 : nanoseconds + 0.5));
}

// ⌈k × 10⁹ / rate⌉, split as k = q × rate + r so that no product overflows 64 bits.
Duration TickRate::TimeOfTick(std::uint64_t tick) const noexcept {
    const std::uint64_t rate = m_TicksPerSecond;
    const std::uint64_t wholeSeconds = tick / rate;
    const std::uint64_t rest = tick % rate;
    RUNIC_ASSERT_MSG(wholeSeconds <=
                         static_cast<std::uint64_t>(std::numeric_limits<Duration::rep>::max()) /
                                 kNanosecondsPerSecond -
                             1,
                     "TickRate::TimeOfTick: the tick is beyond Duration's range");
    const std::uint64_t nanoseconds =
        wholeSeconds * kNanosecondsPerSecond + (rest * kNanosecondsPerSecond + rate - 1) / rate;
    return Duration(static_cast<Duration::rep>(nanoseconds));
}

// ⌊t × rate / 10⁹⌋, split as t = s × 10⁹ + n for the same reason.
std::uint64_t TickRate::TicksIn(Duration elapsed) const noexcept {
    RUNIC_ASSERT_MSG(elapsed.count() >= 0, "TickRate::TicksIn: elapsed time is negative");
    const auto nanoseconds = static_cast<std::uint64_t>(elapsed.count());
    const std::uint64_t seconds = nanoseconds / kNanosecondsPerSecond;
    const std::uint64_t rest = nanoseconds % kNanosecondsPerSecond;
    return seconds * m_TicksPerSecond + rest * m_TicksPerSecond / kNanosecondsPerSecond;
}

} // namespace Runic::Core
