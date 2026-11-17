// RunicServer: the Engine's own game-agnostic server (RunicDocs › Repositories, build targets and
// processes › Processes). Engine slices show their server results here; it is never shipped.
//
//   RunicServer
//       the fixed loop at the master rate (60 Hz) with the Simulation layer on it, until SIGINT
//       or SIGTERM
//   RunicServer --quit-after 120
//       stops after 120 frames (about 2 s) and reports the ticks and the time of every clock
//       domain
//   RunicServer --quit-after 120 --layer-demo
//       slice 1: neutral probe layers show a push, a replacement and a pop, each applied at a
//       frame's safe point (the CTest programs.runic_server)
//   RunicServer --quit-after 120 --clock-demo --set simulation.workers=2
//       slice 3: a neutral probe system in every slot of the scheduler says when it first runs,
//       which shows the order of a master step (the CTest programs.runic_server_clock)
//   RunicServer --config server.cfg
//       settings come from their defaults, then the file, then RUNIC_CFG_* environment variables,
//       then --set key=value; a problem in any of them stops the server with exit code 1
//
// The Engine's server layers so far: Simulation (slice 3). Network and Diagnostics arrive in
// later slices.

#include "Application.hpp"
#include "EntryPoint.hpp"
#include "Log.hpp"
#include "MetricsRegistry.hpp"
#include "ProcessConfig.hpp"
#include "SimulationLayer.hpp"
#include "SimulationRuntime.hpp"
#include "SimulationSettings.hpp"
#include "Time.hpp"

#include <array>
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
using Runic::Core::MetricsRegistry;
using Runic::Core::ProcessConfig;
using Runic::Simulation::ScheduleSlot;
using Runic::Simulation::SimulationLayer;
using Runic::Simulation::SimulationRuntime;
using Runic::Simulation::TickContext;
using Runic::Simulation::TickDomain;

constexpr const char *kProgramName = "RunicServer";
// RunicServer's own flag: the layer demo of slice check 2.
constexpr std::string_view kLayerDemoFlag = "--layer-demo";

// RunicServer's own flag: the clock demo of slice 3.
constexpr std::string_view kClockDemoFlag = "--clock-demo";

// The clock demo: one neutral probe system in every slot of the scheduler.
constexpr std::string_view kClockProbeName = "engine:clock_probe";
constexpr std::array<ScheduleSlot, Runic::Simulation::kScheduleSlotCount> kClockDemoSlots{
    ScheduleSlot::Hz20Producers, ScheduleSlot::Hz10Producers, ScheduleSlot::Hz30Realtime,
    ScheduleSlot::Hz20Consumers, ScheduleSlot::Hz10Consumers, ScheduleSlot::Hz60Hook};

// The layer that stops a server whose configuration has a problem.
constexpr const char *kConfigurationLayerName = "Configuration";

// For the report of a domain's time at stop.
constexpr double kMicrosecondsPerSecond = 1'000'000.0;
constexpr double kP99 = 0.99;

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

// A layer that never attaches: the host reports its error and stops with kLayerFailedExit, which
// is how a server with a bad configuration ends before it runs a frame.
class ConfigurationErrorLayer final : public Layer {
  public:
    explicit ConfigurationErrorLayer(std::size_t problems)
        : Layer(kConfigurationLayerName), m_Problems(problems) {}

    Runic::Core::Result<void> OnAttach(LayerContext &) override {
        return Runic::Core::Fail(std::to_string(m_Problems) +
                                 " problem(s) in the settings, listed above");
    }

  private:
    std::size_t m_Problems;
};

// What the server's layers record into and read from. A base of the application, declared before
// Application, so it outlives the layer stack.
struct ServerServices {
    ProcessConfig config;
    MetricsRegistry metrics;
};

class RunicServerApplication final : private ServerServices, public Application {
  public:
    explicit RunicServerApplication(ApplicationSpec spec)
        : Application(std::move(spec)), m_Start(Runic::Core::MonotonicNow()) {}

    ProcessConfig &Config() noexcept { return config; }
    MetricsRegistry &Metrics() noexcept { return metrics; }

    void OnStopped(int exitCode, std::uint64_t frames) override {
        const double seconds = Runic::Core::ToSeconds(Runic::Core::MonotonicNow() - m_Start);
        if (exitCode == kCleanExit) {
            ReportClockDomains();
            RUNIC_LOG_INFO("RUNIC: {} stopped cleanly after {} frames in {:.1f} s", Spec().name,
                           frames, seconds);
        } else {
            RUNIC_LOG_ERROR("{} stopped with exit code {} after {} frames", Spec().name, exitCode,
                            frames);
        }
    }

  private:
    // The runnable result of slice 3: the ticks each clock domain ran and what they cost, read
    // from the metrics registry the way an exporter would.
    void ReportClockDomains() {
        for (std::uint32_t index = 0; index < metrics.Count(); ++index) {
            const Runic::Core::MetricSnapshot metric = metrics.Read(index);
            if (metric.kind != Runic::Core::MetricKind::Timing ||
                metric.name != Runic::Simulation::kTickTimeMetric) {
                continue;
            }
            const Runic::Core::TimingSnapshot &time = metric.timing;
            RUNIC_LOG_INFO("RUNIC: clock domain {}: {} ticks, mean {:.1f} us, p99 {:.1f} us, max "
                           "{:.1f} us",
                           metric.domain, time.count,
                           Runic::Core::ToSeconds(time.Mean()) * kMicrosecondsPerSecond,
                           Runic::Core::ToSeconds(time.Percentile(kP99)) * kMicrosecondsPerSecond,
                           Runic::Core::ToSeconds(time.max) * kMicrosecondsPerSecond);
        }
    }

    Runic::Core::TimePoint m_Start;
};

// The clock demo: every slot's probe says when it first runs, so the log shows the order of a
// master step.
void AddClockProbes(SimulationRuntime &runtime) {
    for (const ScheduleSlot slot : kClockDemoSlots) {
        runtime.AddSystem({kClockProbeName, slot, [slot](const TickContext &context) {
                               if (context.domainTick != 0) {
                                   return;
                               }
                               RUNIC_LOG_INFO(
                                   "RUNIC: {} ran in {} (master tick {}, {} tick {}, dt {:.4f} s)",
                                   kClockProbeName, Runic::Simulation::ScheduleSlotName(slot),
                                   context.masterTick.Value(),
                                   Runic::Simulation::TickDomainName(context.domain),
                                   context.domainTick, context.dt);
                           }});
    }
}

} // namespace

std::unique_ptr<Application> Runic::Application::CreateApplication(const CommandLine &command) {
    ApplicationSpec spec{kProgramName, LoopPolicy::Fixed(Core::kMasterTicksPerSecond),
                         command.QuitAfterFrames().value_or(ApplicationSpec::kRunUntilStopped)};
    auto application = std::make_unique<RunicServerApplication>(std::move(spec));
    RUNIC_LOG_INFO("RUNIC: {} started (fixed {} Hz)", kProgramName, Core::kMasterTicksPerSecond);

    // Settings: every module the server composes declares its own, then the process's sources
    // are read once. A problem stops the server before its first frame.
    ProcessConfig &config = application->Config();
    const Simulation::SimulationSettings simulationSettings =
        Simulation::DeclareSimulationSettings(config);
    if (const Core::Result<void> loaded = config.LoadForProcess(command.Arguments()); !loaded) {
        for (const Core::ConfigProblem &problem : config.Problems()) {
            RUNIC_LOG_ERROR("{}: {}", problem.source, problem.message);
        }
        application->PushLayer(std::make_unique<ConfigurationErrorLayer>(config.Problems().size()));
        return application;
    }
    for (std::uint32_t index = 0; index < config.Count(); ++index) {
        const Core::SettingInfo setting = config.Describe(index);
        RUNIC_LOG_INFO("RUNIC: setting {} = {} ({})", setting.key, setting.value,
                       Core::ConfigSourceName(setting.source));
    }

    // The Simulation layer first: the server's clock and scheduler.
    Simulation::SimulationSpec simulation =
        Simulation::SimulationSpecFrom(config, simulationSettings);
    simulation.metrics = &application->Metrics();
    auto runtime = std::make_unique<SimulationRuntime>(simulation);
    if (command.Has(kClockDemoFlag)) {
        AddClockProbes(*runtime);
    }
    application->PushLayer(std::make_unique<SimulationLayer>(
        std::move(runtime), Simulation::SimulationLayerSpecFrom(config, simulationSettings)));

    if (command.Has(kLayerDemoFlag)) {
        application->PushLayer(
            std::make_unique<ProbeLayer>(kBaseProbeName, ProbeRole::DrivesTheDemo));
    }
    return application;
}

// The Engine owns main(): it is defined here, once, and hands over to the Engine.
RUNIC_ENTRY_POINT()
