// ApplicationHost (ApplicationHost.hpp): the loop, the frame and the one safe point per frame.

#include "ApplicationHost.hpp"

#include "Assert.hpp"
#include "Log.hpp"

#include <utility>

namespace Runic::Application {

// What layers reach of the host; it forwards to the host and to the application's stack.
class ApplicationHost::HostContext final : public LayerContext {
  public:
    explicit HostContext(ApplicationHost &host) : m_Host(host) {}

    const CommandLine &Command() const override { return m_Host.m_Command; }
    std::uint64_t FramesRun() const override { return m_Host.m_Frames; }

    void PostEvent(const AppEvent &event) override {
        if (!m_Host.m_Events.TryPush(event) && m_Host.m_DroppedEvents++ == 0) {
            RUNIC_LOG_WARNING("{}: the event queue is full ({} events), so events are dropped",
                              m_Host.m_Application->Spec().name, kEventQueueCapacity);
        }
    }

    void RequestStop(int exitCode) override { m_Host.RequestStop(exitCode); }
    void PushLayer(std::unique_ptr<Layer> layer) override { Stack().PushLayer(std::move(layer)); }
    void PushOverlay(std::unique_ptr<Layer> overlay) override {
        Stack().PushOverlay(std::move(overlay));
    }
    void PopLayer(const Layer &layer) override { Stack().PopLayer(layer); }
    void ReplaceLayer(const Layer &current, std::unique_ptr<Layer> replacement) override {
        Stack().ReplaceLayer(current, std::move(replacement));
    }

  private:
    LayerStack &Stack() { return m_Host.m_Application->Layers(); }

    ApplicationHost &m_Host;
};

ApplicationHost::ApplicationHost(std::unique_ptr<Application> application, CommandLine command,
                                 std::unique_ptr<LoopClock> clock)
    : m_Application(std::move(application)), m_Command(std::move(command)),
      m_Clock(std::move(clock)), m_Context(std::make_unique<HostContext>(*this)) {
    RUNIC_ASSERT_MSG(m_Application != nullptr, "ApplicationHost needs an application");
    if (m_Clock == nullptr) {
        m_Clock = MakeSystemClock();
    }
}

ApplicationHost::~ApplicationHost() { Shutdown(); }

int ApplicationHost::Run() {
    const ApplicationSpec &spec = m_Application->Spec();
    if (Core::Result<void> started = Start(); !started) {
        RUNIC_LOG_ERROR("{} could not start: {}", spec.name, started.GetError().message);
        Shutdown();
        return ExitCode();
    }
    const Core::TimePoint start = m_Clock->Now();
    Core::TimePoint previous = start;
    while (!StopRequested()) {
        FrameTime time;
        time.frame = m_Frames;
        if (spec.loop.IsFixed()) {
            const Core::TickRate rate = spec.loop.Rate();
            const Core::Duration due = rate.TimeOfTick(m_Frames);
            m_Clock->SleepUntil(start + due);
            time.sinceStart = due;
            time.delta = m_Frames == 0 ? Core::Duration(0) : due - rate.TimeOfTick(m_Frames - 1);
        } else {
            const Core::TimePoint now = m_Clock->Now();
            time.sinceStart = now - start;
            time.delta = m_Frames == 0 ? Core::Duration(0) : now - previous;
            previous = now;
        }
        RunFrame(time);
        if (spec.maxFrames != 0 && m_Frames >= spec.maxFrames) {
            RequestStop(0);
        }
    }
    Shutdown();
    return ExitCode();
}

Core::Result<void> ApplicationHost::Start() {
    if (m_Started) {
        return {};
    }
    m_Started = true;
    Core::Result<void> attached = m_Application->Layers().ApplyPendingChanges(*m_Context);
    if (!attached) {
        RequestStop(1);
    }
    return attached;
}

void ApplicationHost::RunFrame(const FrameTime &time) {
    LayerStack &layers = m_Application->Layers();
    // The events queued before this frame, top-down; one posted meanwhile waits for the next frame.
    for (std::size_t queued = m_Events.Size(); queued > 0; --queued) {
        AppEvent event;
        m_Events.TryPop(event);
        layers.DispatchEvent(event);
        if (!event.handled && event.Is<QuitRequestEvent>()) {
            RequestStop(0);
        }
    }
    layers.UpdateBottomUp(time);
    layers.RenderBottomUp();
    ++m_Frames;
    // The frame's one safe point.
    if (layers.HasPendingChanges()) {
        if (Core::Result<void> applied = layers.ApplyPendingChanges(*m_Context); !applied) {
            RUNIC_LOG_ERROR("{}: {}", m_Application->Spec().name, applied.GetError().message);
            RequestStop(1);
        }
    }
}

void ApplicationHost::Shutdown() {
    if (m_ShutDown) {
        return;
    }
    m_ShutDown = true;
    m_Application->Layers().DetachAll();
    m_Application->OnStopped(ExitCode(), m_Frames);
}

void ApplicationHost::RequestStop(int exitCode) noexcept {
    bool expected = false;
    if (m_StopRequested.compare_exchange_strong(expected, true)) {
        m_ExitCode.store(exitCode);
    }
}

bool ApplicationHost::StopRequested() const noexcept { return m_StopRequested.load(); }
int ApplicationHost::ExitCode() const noexcept { return m_ExitCode.load(); }
std::uint64_t ApplicationHost::FramesRun() const noexcept { return m_Frames; }
std::uint64_t ApplicationHost::DroppedEvents() const noexcept { return m_DroppedEvents; }
LayerContext &ApplicationHost::Context() noexcept { return *m_Context; }
Application &ApplicationHost::App() noexcept { return *m_Application; }

} // namespace Runic::Application
