// RunicServer: the Engine's own game-agnostic server (RunicDocs › Repositories, build targets and
// processes › Processes). Engine slices show their server results here; it is never shipped.
//
//   RunicServer                              the fixed 60 Hz loop, until SIGINT or SIGTERM
//   RunicServer --quit-after 120             stops after 120 frames (about 2 s)
//   RunicServer --quit-after 120 --layer-demo
//                                            slice 1: neutral probe layers show a push, a replace
//                                            and a pop, each applied at a frame's safe point (the
//                                            CTest programs.runic_server)
//
// Slice 1 has no Engine server layer yet (Network, Simulation and Diagnostics arrive in later
// slices).

#include "Application.hpp"
#include "EntryPoint.hpp"
#include "Log.hpp"
#include "Time.hpp"

#include <memory>
#include <string>
#include <utility>

namespace {

using Runic::Application::Application;
using Runic::Application::ApplicationSpec;
using Runic::Application::CommandLine;
using Runic::Application::FrameTime;
using Runic::Application::Layer;
using Runic::Application::LayerContext;
using Runic::Application::LoopPolicy;

// A neutral test layer: it reports when it attaches and detaches. The demo's base layer also asks
// for the next change at fixed frames: a push after 30 frames, a replacement after 60 and a pop
// after 90.
class ProbeLayer final : public Layer {
  public:
    ProbeLayer(std::string name, bool drivesDemo)
        : Layer(std::move(name)), m_DrivesDemo(drivesDemo) {}

    Runic::Core::Result<void> OnAttach(LayerContext &context) override {
        m_Context = &context;
        RUNIC_LOG_INFO("RUNIC: {} attached after {} frames", Name(), context.FramesRun());
        return {};
    }

    void OnDetach() override {
        RUNIC_LOG_INFO("RUNIC: {} detached after {} frames", Name(), m_Context->FramesRun());
    }

    void OnUpdate(const FrameTime &time) override {
        if (!m_DrivesDemo) {
            return;
        }
        if (time.frame + 1 == 30) {
            auto pushed = std::make_unique<ProbeLayer>("engine:probe_b", false);
            m_Demo = pushed.get();
            m_Context->PushLayer(std::move(pushed));
        } else if (time.frame + 1 == 60 && m_Demo != nullptr) {
            auto replacement = std::make_unique<ProbeLayer>("engine:probe_c", false);
            const Layer *current = m_Demo;
            m_Demo = replacement.get();
            m_Context->ReplaceLayer(*current, std::move(replacement));
        } else if (time.frame + 1 == 90 && m_Demo != nullptr) {
            m_Context->PopLayer(*m_Demo);
            m_Demo = nullptr;
        }
    }

  private:
    bool m_DrivesDemo;
    LayerContext *m_Context = nullptr;
    const Layer *m_Demo = nullptr;
};

class RunicServerApplication final : public Application {
  public:
    explicit RunicServerApplication(ApplicationSpec spec)
        : Application(std::move(spec)), m_Start(Runic::Core::MonotonicNow()) {}

    void OnStopped(int exitCode, std::uint64_t frames) override {
        const double seconds = Runic::Core::ToSeconds(Runic::Core::MonotonicNow() - m_Start);
        if (exitCode == 0) {
            RUNIC_LOG_INFO("RUNIC: RunicServer stopped cleanly after {} frames in {:.1f} s", frames,
                           seconds);
        } else {
            RUNIC_LOG_ERROR("RunicServer stopped with exit code {} after {} frames", exitCode,
                            frames);
        }
    }

  private:
    Runic::Core::TimePoint m_Start;
};

} // namespace

std::unique_ptr<Application> Runic::Application::CreateApplication(const CommandLine &command) {
    ApplicationSpec spec{"RunicServer", LoopPolicy::Fixed(60),
                         command.QuitAfterFrames().value_or(0)};
    auto application = std::make_unique<RunicServerApplication>(std::move(spec));
    if (command.Has("--layer-demo")) {
        application->PushLayer(std::make_unique<ProbeLayer>("engine:probe_a", true));
    }
    RUNIC_LOG_INFO("RUNIC: RunicServer started (fixed 60 Hz)");
    return application;
}

// The Engine owns main(): it is defined here, once, and hands over to the Engine.
RUNIC_ENTRY_POINT()
