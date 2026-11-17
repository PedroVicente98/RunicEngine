#pragma once

#include <array>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <string_view>

#include "Assert.hpp"
#include "PlainIds.hpp"
#include "Time.hpp"

// The clock domains of a simulation: a 60 Hz master clock and the 30, 20 and 10 Hz domains that
// tick on every 2nd, 3rd and 6th master tick (RunicDocs › Simulation core clock and threads ›
// Multi-rate fixed-step scheduler). This is where the tick rates live. A duration in simulation
// code is a time, or a DomainTickCount: a number of ticks that says which domain's ticks it counts.

namespace Runic::Simulation {

enum class TickDomain : std::uint8_t {
    Hz60, // the master clock; no mandatory gameplay work
    Hz30, // the realtime phase: commands, movement, physics, combat queries, replication
    Hz20, // AI decisions and perception
    Hz10, // slow scans, world state, quest and loot consumers, maintenance
};

inline constexpr std::size_t kTickDomainCount = 4;

// Every domain, from the fastest to the slowest.
inline constexpr std::array<TickDomain, kTickDomainCount> kTickDomains{
    TickDomain::Hz60, TickDomain::Hz30, TickDomain::Hz20, TickDomain::Hz10};

namespace Detail {
inline constexpr std::array<std::uint32_t, kTickDomainCount> kMasterTicksPerTick{1, 2, 3, 6};
inline constexpr std::array<std::string_view, kTickDomainCount> kTickDomainNames{"hz60", "hz30",
                                                                                 "hz20", "hz10"};
} // namespace Detail

// The position of a domain in kTickDomains, for arrays that hold one value per domain.
constexpr std::size_t IndexOf(TickDomain domain) noexcept {
    return static_cast<std::size_t>(domain);
}

// Master ticks from one tick of the domain to the next: 1, 2, 3 or 6.
constexpr std::uint32_t MasterTicksPerTick(TickDomain domain) noexcept {
    return Detail::kMasterTicksPerTick[IndexOf(domain)];
}

// 60, 30, 20 or 10.
constexpr std::uint32_t TicksPerSecond(TickDomain domain) noexcept {
    return Core::kMasterTicksPerSecond / MasterTicksPerTick(domain);
}

constexpr Core::TickRate RateOf(TickDomain domain) noexcept {
    return Core::TickRate::Hz(TicksPerSecond(domain));
}

// The domain's fixed step in seconds, for 32-bit gameplay math. Never accumulate it into a time:
// the time of a tick comes from its number (TickContext::time).
constexpr float StepSeconds(TickDomain domain) noexcept {
    return static_cast<float>(MasterTicksPerTick(domain)) /
           static_cast<float>(Core::kMasterTicksPerSecond);
}

// "hz60", "hz30", "hz20" or "hz10": the label the domain's metrics carry.
constexpr std::string_view TickDomainName(TickDomain domain) noexcept {
    return Detail::kTickDomainNames[IndexOf(domain)];
}

// True on the master ticks the domain ticks on. Every domain ticks on master tick 0.
constexpr bool IsDue(TickDomain domain, Core::TickId masterTick) noexcept {
    return masterTick.Value() % MasterTicksPerTick(domain) == 0;
}

// The number of the domain tick that runs on a master tick the domain is due on.
constexpr std::uint64_t DomainTickAt(TickDomain domain, Core::TickId masterTick) noexcept {
    return masterTick.Value() / MasterTicksPerTick(domain);
}

// A number of ticks of one domain: how gameplay counts a duration in ticks ("45 ticks at 30 Hz").
// It is built from a domain and a count, or from a time, never from a bare number, and two counts
// of different domains do not mix: convert one with In() first.
class DomainTickCount {
  public:
    constexpr DomainTickCount(TickDomain domain, std::uint64_t ticks) noexcept
        : m_Ticks(ticks), m_Domain(domain) {}

    // The fewest ticks of the domain that last at least the duration (never shorter than asked).
    static DomainTickCount Covering(TickDomain domain, Core::Duration duration) noexcept;

    constexpr TickDomain Domain() const noexcept { return m_Domain; }
    constexpr std::uint64_t Ticks() const noexcept { return m_Ticks; }

    // The time the ticks last: exact, from the domain's rate.
    Core::Duration ToDuration() const noexcept;

    // The fewest ticks of another domain that last at least as long.
    DomainTickCount In(TickDomain domain) const noexcept;

    friend constexpr DomainTickCount operator+(DomainTickCount left,
                                               DomainTickCount right) noexcept {
        RUNIC_ASSERT_MSG(left.m_Domain == right.m_Domain, "tick counts of two domains were mixed");
        return DomainTickCount(left.m_Domain, left.m_Ticks + right.m_Ticks);
    }
    // Asserts that right is not more than left.
    friend constexpr DomainTickCount operator-(DomainTickCount left,
                                               DomainTickCount right) noexcept {
        RUNIC_ASSERT_MSG(left.m_Domain == right.m_Domain, "tick counts of two domains were mixed");
        RUNIC_ASSERT(left.m_Ticks >= right.m_Ticks);
        return DomainTickCount(left.m_Domain, left.m_Ticks - right.m_Ticks);
    }
    friend constexpr bool operator==(DomainTickCount left, DomainTickCount right) noexcept {
        RUNIC_ASSERT_MSG(left.m_Domain == right.m_Domain, "tick counts of two domains were mixed");
        return left.m_Ticks == right.m_Ticks;
    }
    friend constexpr std::strong_ordering operator<=>(DomainTickCount left,
                                                      DomainTickCount right) noexcept {
        RUNIC_ASSERT_MSG(left.m_Domain == right.m_Domain, "tick counts of two domains were mixed");
        return left.m_Ticks <=> right.m_Ticks;
    }

  private:
    std::uint64_t m_Ticks;
    TickDomain m_Domain;
};

} // namespace Runic::Simulation
