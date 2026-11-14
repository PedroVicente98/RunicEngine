// Loop policies and the system clock (LoopPolicy.hpp).

#include "LoopPolicy.hpp"

#include "Assert.hpp"

#include <thread>

namespace Runic::Application {

namespace {

class SystemClock final : public LoopClock {
  public:
    Core::TimePoint Now() override { return Core::MonotonicNow(); }
    void SleepUntil(Core::TimePoint deadline) override { std::this_thread::sleep_until(deadline); }
};

} // namespace

LoopPolicy::LoopPolicy(bool fixed, Core::TickRate rate) noexcept : m_Fixed(fixed), m_Rate(rate) {}

LoopPolicy LoopPolicy::Fixed(std::uint32_t framesPerSecond) noexcept {
    return LoopPolicy(true, Core::TickRate::Hz(framesPerSecond));
}

LoopPolicy LoopPolicy::PacedByTopLayer() noexcept {
    return LoopPolicy(false, Core::TickRate::Hz(1));
}

bool LoopPolicy::IsFixed() const noexcept { return m_Fixed; }

Core::TickRate LoopPolicy::Rate() const noexcept {
    RUNIC_ASSERT_MSG(m_Fixed, "LoopPolicy::Rate() on a loop paced by the top layer");
    return m_Rate;
}

std::unique_ptr<LoopClock> MakeSystemClock() { return std::make_unique<SystemClock>(); }

} // namespace Runic::Application
