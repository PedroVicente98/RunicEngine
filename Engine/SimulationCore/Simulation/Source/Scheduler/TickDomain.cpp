// The clock domains (TickDomain.hpp): tick counts and the times they last.

#include "TickDomain.hpp"

namespace Runic::Simulation {

DomainTickCount DomainTickCount::Covering(TickDomain domain, Core::Duration duration) noexcept {
    RUNIC_ASSERT(duration >= Core::Duration::zero());
    const Core::TickRate rate = RateOf(domain);
    // TicksIn rounds down: one more tick when those do not last the whole duration.
    std::uint64_t ticks = rate.TicksIn(duration);
    if (rate.TimeOfTick(ticks) < duration) {
        ++ticks;
    }
    return DomainTickCount(domain, ticks);
}

Core::Duration DomainTickCount::ToDuration() const noexcept {
    return RateOf(m_Domain).TimeOfTick(m_Ticks);
}

DomainTickCount DomainTickCount::In(TickDomain domain) const noexcept {
    if (domain == m_Domain) {
        return *this;
    }
    // Both domains count whole master ticks, so the conversion is exact integer arithmetic:
    // the fewest ticks of the other domain that span this count's master ticks.
    const std::uint64_t masterTicks = m_Ticks * MasterTicksPerTick(m_Domain);
    const std::uint64_t per = MasterTicksPerTick(domain);
    return DomainTickCount(domain, (masterTicks + per - 1) / per);
}

} // namespace Runic::Simulation
