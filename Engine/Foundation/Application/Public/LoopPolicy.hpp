#pragma once

#include <cstdint>
#include <memory>

#include "Time.hpp"

namespace Runic::Application {

// How ApplicationHost::Run paces frames (RunicDocs › Application and LayerStack › Loop policies).
class LoopPolicy {
  public:
    // Frame n starts TickRate::TimeOfTick(n) after the first, and the host sleeps until then:
    // GameServer, RunicServer, RunicBot. A late frame runs at once; the schedule never drifts,
    // because every deadline comes from n.
    static LoopPolicy Fixed(std::uint32_t framesPerSecond) noexcept;
    // No sleep: the top layer paces the loop (Godot's iteration waits for vsync or the editor's
    // low-processor mode): GameClient, RunicClient, RunicEditor.
    static LoopPolicy PacedByTopLayer() noexcept;

    bool IsFixed() const noexcept;
    Core::TickRate Rate() const noexcept; // the fixed rate; asserts IsFixed()

  private:
    LoopPolicy(bool fixed, Core::TickRate rate) noexcept;

    bool m_Fixed;
    Core::TickRate m_Rate;
};

// The clock the host paces frames with. Tests replace it to run a fixed loop without waiting.
class LoopClock {
  public:
    virtual ~LoopClock() = default;
    virtual Core::TimePoint Now() = 0;
    virtual void SleepUntil(Core::TimePoint deadline) = 0;
};

// Core::MonotonicNow, and a sleep until the deadline.
std::unique_ptr<LoopClock> MakeSystemClock();

} // namespace Runic::Application
