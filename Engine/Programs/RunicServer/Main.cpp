// RunicServer: the Engine's own game-agnostic server (RunicDocs › Repositories, build targets and
// processes › Processes). Engine slices show their server results here; it is never shipped.
//
//   RunicServer
//       the fixed loop at the master rate (60 Hz), until SIGINT or SIGTERM
//   RunicServer --quit-after 120
//       stops after 120 frames (about 2 s)
//   RunicServer --quit-after 120 --layer-demo
//       slice 1: neutral probe layers show a push, a replacement and a pop, each applied at a
//       frame's safe point (the CTest programs.runic_server)
//
// Slice 1 has no Engine server layer yet (Network, Simulation and Diagnostics arrive in later
// slices).

#include "Application.hpp"
#include "EntryPoint.hpp"
#include "Log.hpp"
#include "Time.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

namespace {

using Runic::Application::Application;
using Runic::Application::ApplicationSpec;
using Runic::Application::CommandLine;
using Runic::Application::FrameTime;
using Runic::Application::kCleanExit;
using Runic::Application::Layer;
using Runic::Application::LayerContext;
using Runic::Application::LoopPolicy;

constexpr const char *kProgramName = "RunicServer";
// RunicServer's own flag: the layer demo of slice check 2.
constexpr std::string_view kLayerDemoFlag = "--layer-demo";

// The layer demo: the base probe, the probe it pushes and the one that replaces it, and how many
// frames have run when each change is asked for.
constexpr const char *kBaseProbeName = "engine:probe_a";
constexpr const char *kPushedProbeName = "engine:probe_b";
constexpr const char *kReplacementProbeName = "engine:probe_c";
constexpr std::uint64_t kPushAfterFrames = 30;
constexpr std::uint64_t kReplaceAfterFrames = 60;
constexpr std::uint64_t kPopAfterFrames = 90;

// Only the base probe asks for the demo's changes.
enum class ProbeRole { DrivesTheDemo, ReportsOnly };

// A neutral test layer: it reports when it attaches and detaches.
class ProbeLayer final : public Layer {
  public:
    ProbeLayer(std::string name, ProbeRole role) : Layer(std::move(name)), m_Role(role) {}

    Runic::Core::Result<void> OnAttach(LayerContext &context) override {
        m_Context = &context;
        RUNIC_LOG_INFO("RUNIC: {} attached after {} frames", Name(), context.FramesRun());
        return {};
    }

    void OnDetach() override {
        RUNIC_LOG_INFO("RUNIC: {} detached after {} frames", Name(), m_Context->FramesRun());
    }

    void OnUpdate(const FrameTime &time) override {
        if (m_Role != ProbeRole::DrivesTheDemo) {
            return;
        }
        const std::uint64_t framesRun = time.frame + 1; // this frame included
        if (framesRun == kPushAfterFrames) {
            auto pushed = std::make_unique<ProbeLayer>(kPushedProbeName, ProbeRole::ReportsOnly);
            m_Demo = pushed.get();
            m_Context->PushLayer(std::move(pushed));
        } else if (framesRun == kReplaceAfterFrames && m_Demo != nullptr) {
            auto replacement =
                std::make_unique<ProbeLayer>(kReplacementProbeName, ProbeRole::ReportsOnly);
            const Layer *current = m_Demo;
            m_Demo = replacement.get();
            m_Context->ReplaceLayer(*current, std::move(replacement));
        } else if (framesRun == kPopAfterFrames && m_Demo != nullptr) {
            m_Context->PopLayer(*m_Demo);
            m_Demo = nullptr;
        }
    }

  private:
    ProbeRole m_Role;
    LayerContext *m_Context = nullptr;
    const Layer *m_Demo = nullptr;
};

class RunicServerApplication final : public Application {
  public:
    explicit RunicServerApplication(ApplicationSpec spec)
        : Application(std::move(spec)), m_Start(Runic::Core::MonotonicNow()) {}

    void OnStopped(int exitCode, std::uint64_t frames) override {
        const double seconds = Runic::Core::ToSeconds(Runic::Core::MonotonicNow() - m_Start);
        if (exitCode == kCleanExit) {
            RUNIC_LOG_INFO("RUNIC: {} stopped cleanly after {} frames in {:.1f} s", Spec().name,
                           frames, seconds);
        } else {
            RUNIC_LOG_ERROR("{} stopped with exit code {} after {} frames", Spec().name, exitCode,
                            frames);
        }
    }

  private:
    Runic::Core::TimePoint m_Start;
};

} // namespace

std::unique_ptr<Application> Runic::Application::CreateApplication(const CommandLine &command) {
    ApplicationSpec spec{kProgramName, LoopPolicy::Fixed(Core::kMasterTicksPerSecond),
                         command.QuitAfterFrames().value_or(ApplicationSpec::kRunUntilStopped)};
    auto application = std::make_unique<RunicServerApplication>(std::move(spec));
    if (command.Has(kLayerDemoFlag)) {
        application->PushLayer(
            std::make_unique<ProbeLayer>(kBaseProbeName, ProbeRole::DrivesTheDemo));
    }
    RUNIC_LOG_INFO("RUNIC: {} started (fixed {} Hz)", kProgramName, Core::kMasterTicksPerSecond);
    return application;
}

// The Engine owns main(): it is defined here, once, and hands over to the Engine.
RUNIC_ENTRY_POINT()
