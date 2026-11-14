#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>

#include "Application.hpp"
#include "CommandLine.hpp"
#include "FixedRingBuffer.hpp"
#include "LoopPolicy.hpp"

namespace Runic::Application {

// Runs one Application (RunicDocs › Application and LayerStack › Entry point): Run repeats RunFrame
// under the loop policy, and RunFrame is one complete frame, so a host that does not own the loop
// can drive one frame at a time.
class ApplicationHost {
  public:
    static constexpr std::size_t kEventQueueCapacity = 256;

    ApplicationHost(std::unique_ptr<Application> application, CommandLine command,
                    std::unique_ptr<LoopClock> clock = MakeSystemClock());
    ~ApplicationHost(); // Shutdown() if it has not run
    ApplicationHost(const ApplicationHost &) = delete;
    ApplicationHost &operator=(const ApplicationHost &) = delete;

    // Start, then frames until a stop is requested (or ApplicationSpec::maxFrames), then Shutdown.
    // Returns the exit code: 0 for a clean stop, 1 when a layer failed to attach.
    int Run();
    // Attaches the layers pushed before Run: the first safe point. On failure the layers that
    // attached are kept for Shutdown, which detaches them, and the exit code becomes 1.
    Core::Result<void> Start();
    // One complete frame: the queued events top-down, updates bottom-up, renders bottom-up, then
    // the safe point.
    void RunFrame(const FrameTime &time);
    // Detaches every layer, top-down, then calls Application::OnStopped. Safe to call twice.
    void Shutdown();

    // Thread-safe and safe in a signal handler; the first request's exit code wins.
    void RequestStop(int exitCode) noexcept;
    bool StopRequested() const noexcept;
    int ExitCode() const noexcept;
    std::uint64_t FramesRun() const noexcept;
    std::uint64_t DroppedEvents() const noexcept; // events the full queue refused
    LayerContext &Context() noexcept;
    Application &App() noexcept;

  private:
    class HostContext;

    std::unique_ptr<Application> m_Application;
    CommandLine m_Command;
    std::unique_ptr<LoopClock> m_Clock;
    std::unique_ptr<HostContext> m_Context;
    Core::FixedRingBuffer<AppEvent, kEventQueueCapacity> m_Events;
    std::atomic<bool> m_StopRequested{false};
    std::atomic<int> m_ExitCode{0};
    std::uint64_t m_Frames = 0;
    std::uint64_t m_DroppedEvents = 0;
    bool m_Started = false;
    bool m_ShutDown = false;
};

} // namespace Runic::Application
