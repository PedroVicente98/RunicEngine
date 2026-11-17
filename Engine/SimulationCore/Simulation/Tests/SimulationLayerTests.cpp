#include "SimulationLayer.hpp"
#include "SimulationSettings.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "CommandLine.hpp"
#include "Log.hpp"
#include "MetricsRegistry.hpp"
#include "ProcessConfig.hpp"

using namespace Runic;
using namespace Runic::Simulation;
using namespace std::chrono_literals;

namespace {

// The part of a host a layer sees. The Simulation layer asks it for nothing.
class TestContext final : public Application::LayerContext {
  public:
    const Application::CommandLine &Command() const override { return m_Command; }
    std::uint64_t FramesRun() const override { return 0; }
    void PostEvent(const Application::AppEvent &) override {}
    void RequestStop(int) override {}
    void PushLayer(std::unique_ptr<Application::Layer>) override {}
    void PushOverlay(std::unique_ptr<Application::Layer>) override {}
    void PopLayer(const Application::Layer &) override {}
    void ReplaceLayer(const Application::Layer &, std::unique_ptr<Application::Layer>) override {}

  private:
    Application::CommandLine m_Command;
};

// Captures log lines while it lives.
class LogCapture {
  public:
    LogCapture() { Core::SetLogSink(&LogCapture::Sink, this); }
    ~LogCapture() { Core::SetLogSink(nullptr, nullptr); }

    std::vector<std::string> Lines(Core::LogLevel level) const {
        std::vector<std::string> lines;
        for (const auto &[lineLevel, text] : m_Lines) {
            if (lineLevel == level) {
                lines.push_back(text);
            }
        }
        return lines;
    }

  private:
    static void Sink(const Core::LogRecord &record, void *user) {
        static_cast<LogCapture *>(user)->m_Lines.emplace_back(record.level, record.message);
    }

    std::vector<std::pair<Core::LogLevel, std::string>> m_Lines;
};

// A layer on a fresh runtime, attached, and a way to update it at a given loop time.
struct Harness {
    explicit Harness(const SimulationLayerSpec &spec = {},
                     Core::MetricsRegistry *metrics = nullptr) {
        SimulationSpec simulation;
        simulation.metrics = metrics;
        layer = std::make_unique<SimulationLayer>(std::make_unique<SimulationRuntime>(simulation),
                                                  spec);
        EXPECT_TRUE(layer->OnAttach(context));
    }

    void UpdateAt(Core::Duration sinceStart) {
        Application::FrameTime time;
        time.frame = frames++;
        time.delta = sinceStart - last;
        time.sinceStart = sinceStart;
        last = sinceStart;
        layer->OnUpdate(time);
    }

    std::uint64_t MasterTicks() const { return layer->Runtime().NextMasterTick().Value(); }

    TestContext context;
    std::unique_ptr<SimulationLayer> layer;
    std::uint64_t frames = 0;
    Core::Duration last{};
};

} // namespace

TEST(SimulationLayer, AFixedSixtyHertzLoopRunsOneStepPerUpdate) {
    const LogCapture log;
    Harness harness;
    EXPECT_EQ(harness.layer->Name(), kSimulationLayerName);
    const Core::TickRate loop = Core::TickRate::Hz(60);

    for (std::uint64_t frame = 0; frame < 600; ++frame) {
        harness.UpdateAt(loop.TimeOfTick(frame)); // the fixed loop's exact frame times
        ASSERT_EQ(harness.MasterTicks(), frame + 1) << "frame " << frame;
        ASSERT_EQ(harness.layer->BacklogTicks(), 0u);
    }

    const SimulationRuntime &runtime = harness.layer->Runtime();
    EXPECT_EQ(runtime.TicksRun(TickDomain::Hz30), 300u);
    EXPECT_EQ(runtime.TicksRun(TickDomain::Hz20), 200u);
    EXPECT_EQ(runtime.TicksRun(TickDomain::Hz10), 100u);
    EXPECT_TRUE(log.Lines(Core::LogLevel::Warning).empty());
}

TEST(SimulationLayer, ASlowerLoopRunsEveryOwedStep) {
    Harness harness;
    const Core::TickRate loop = Core::TickRate::Hz(30);
    for (std::uint64_t frame = 0; frame < 300; ++frame) {
        harness.UpdateAt(loop.TimeOfTick(frame));
        ASSERT_EQ(harness.MasterTicks(), 2 * frame + 1)
            << "frame " << frame; // tick 0, then two each
    }
    EXPECT_EQ(harness.layer->BacklogTicks(), 0u);
}

TEST(SimulationLayer, AFasterLoopRunsAStepOnlyWhenOneIsDue) {
    Harness harness;
    const Core::TickRate loop = Core::TickRate::Hz(120);
    for (std::uint64_t frame = 0; frame < 480; ++frame) {
        harness.UpdateAt(loop.TimeOfTick(frame));
        ASSERT_EQ(harness.MasterTicks(), frame / 2 + 1) << "frame " << frame;
    }
}

TEST(SimulationLayer, TheLoopsTimeBeforeTheFirstUpdateDoesNotCount) {
    Harness harness;
    harness.UpdateAt(5s);                 // the layer was pushed five seconds into the run
    EXPECT_EQ(harness.MasterTicks(), 1u); // master tick 0, not 300 owed ticks
    harness.UpdateAt(5s + 16ms);
    EXPECT_EQ(harness.MasterTicks(), 1u); // tick 1 is due 16.67 ms after tick 0
    harness.UpdateAt(5s + 17ms);
    EXPECT_EQ(harness.MasterTicks(), 2u);
}

TEST(SimulationLayer, AStallIsCaughtUpOverLaterUpdatesAndNoStepIsSkipped) {
    const LogCapture log;
    Core::MetricsRegistry metrics;
    Harness harness({}, &metrics); // up to kDefaultMaxTicksPerUpdate steps per update
    const Core::GaugeId backlog = metrics.AddGauge({kBacklogMetric, "hz60", ""});
    const Core::TickRate loop = Core::TickRate::Hz(60);

    harness.UpdateAt(0ms);
    EXPECT_EQ(harness.MasterTicks(), 1u);

    // The process stalls for a second: 60 more ticks are due, and one update runs five of them.
    harness.UpdateAt(1s);
    EXPECT_EQ(harness.MasterTicks(), 1u + kDefaultMaxTicksPerUpdate);
    EXPECT_EQ(harness.layer->BacklogTicks(), 60u - kDefaultMaxTicksPerUpdate);
    EXPECT_DOUBLE_EQ(metrics.Read(backlog), 55.0);
    ASSERT_EQ(log.Lines(Core::LogLevel::Warning).size(), 1u);
    EXPECT_NE(log.Lines(Core::LogLevel::Warning)[0].find("55 master step(s) owed"),
              std::string::npos);

    // The loop goes on at 60 Hz: each update owes one more and runs five, until none is owed.
    std::uint64_t frame = 60;
    while (harness.layer->BacklogTicks() > 0) {
        ++frame;
        const std::uint64_t before = harness.layer->BacklogTicks();
        harness.UpdateAt(loop.TimeOfTick(frame));
        ASSERT_LT(harness.layer->BacklogTicks(), before);
        ASSERT_LT(frame, 200u);
    }
    // Every tick that was ever due has run: none was dropped to catch up.
    EXPECT_EQ(harness.MasterTicks(), frame + 1);
    EXPECT_DOUBLE_EQ(metrics.Read(backlog), 0.0);
    EXPECT_EQ(log.Lines(Core::LogLevel::Warning).size(), 1u); // one warning for the whole stall
}

TEST(SimulationLayer, AnUnevenLoopNeverDriftsOverALongRun) {
    SimulationLayerSpec spec;
    spec.maxTicksPerUpdate = Core::kMasterTicksPerSecond;
    Harness harness(spec);
    const Core::TickRate master = Core::TickRate::Hz(60);

    // Two simulated hours of frames between 1 and 40 ms long, from a fixed pseudo-random sequence.
    Core::Duration now{};
    std::uint32_t random = 12345;
    harness.UpdateAt(now);
    while (now < Core::Duration(2h)) {
        random = random * 1664525u + 1013904223u;
        now += Core::Duration(1ms) + Core::Duration(random % 39'000'000);
        harness.UpdateAt(now);
        // After every update the simulation has run exactly the ticks whose time has come.
        ASSERT_EQ(harness.MasterTicks(), master.TicksIn(now) + 1);
    }
    EXPECT_EQ(harness.layer->BacklogTicks(), 0u);
    EXPECT_GE(harness.MasterTicks(), 432'000u); // two hours at 60 Hz
    EXPECT_LE(harness.layer->Runtime().Time(), now + master.TimeOfTick(1));
}

TEST(SimulationLayer, DetachReportsTheTicksOfEveryDomain) {
    const LogCapture log;
    Harness harness;
    for (std::uint64_t frame = 0; frame < 120; ++frame) {
        harness.UpdateAt(Core::TickRate::Hz(60).TimeOfTick(frame));
    }
    harness.layer->OnDetach();

    const std::vector<std::string> lines = log.Lines(Core::LogLevel::Info);
    ASSERT_EQ(lines.size(), 2u);
    EXPECT_EQ(lines[0],
              "RUNIC: Simulation layer attached: simulation 1, 1 worker(s), up to 5 step(s) "
              "per update");
    EXPECT_EQ(lines[1], "RUNIC: Simulation layer ran 120 master ticks (hz60 120, hz30 60, hz20 40, "
                        "hz10 20), 0 owed");
}

TEST(SimulationSettings, AreDeclaredWithTheirDefaults) {
    Core::ProcessConfig config;
    const SimulationSettings settings = DeclareSimulationSettings(config);
    ASSERT_TRUE(config.Load({}));

    const SimulationSpec simulation = SimulationSpecFrom(config, settings);
    EXPECT_EQ(simulation.workers, Core::kSingleWorker);
    EXPECT_EQ(simulation.id, kStandaloneSimulation);
    EXPECT_EQ(SimulationLayerSpecFrom(config, settings).maxTicksPerUpdate,
              kDefaultMaxTicksPerUpdate);
    EXPECT_EQ(config.Describe(settings.workers.Index()).key, kWorkersSetting);
    EXPECT_EQ(config.Describe(settings.maxTicksPerUpdate.Index()).key, kMaxTicksPerUpdateSetting);
}

TEST(SimulationSettings, ComeFromTheConfiguration) {
    Core::ProcessConfig config;
    const SimulationSettings settings = DeclareSimulationSettings(config);
    const std::vector<std::string> environment = {"RUNIC_CFG_SIMULATION_MAX_TICKS_PER_UPDATE=12"};
    const std::vector<std::string> arguments = {"--set", "simulation.workers=4"};
    Core::ConfigInputs inputs;
    inputs.environment = environment;
    inputs.arguments = arguments;
    ASSERT_TRUE(config.Load(inputs));

    const SimulationSpec simulation = SimulationSpecFrom(config, settings);
    EXPECT_EQ(simulation.workers, 4u);
    SimulationRuntime runtime(simulation);
    EXPECT_EQ(runtime.Workers().WorkerCount(), 4u);
    EXPECT_EQ(SimulationLayerSpecFrom(config, settings).maxTicksPerUpdate, 12u);
}

TEST(SimulationSettings, AWorkerCountThePoolCannotHaveIsReported) {
    Core::ProcessConfig config;
    const SimulationSettings settings = DeclareSimulationSettings(config);
    const std::vector<std::string> arguments = {"--set", "simulation.workers=0", "--set",
                                                "simulation.max_ticks_per_update=61"};
    Core::ConfigInputs inputs;
    inputs.arguments = arguments;
    const Core::Result<void> loaded = config.Load(inputs);

    ASSERT_FALSE(loaded);
    ASSERT_EQ(config.Problems().size(), 2u);
    EXPECT_EQ(config.Problems()[0].message, "simulation.workers: 0 is outside 1 to 64");
    EXPECT_EQ(config.Problems()[1].message,
              "simulation.max_ticks_per_update: 61 is outside 1 to 60");
    EXPECT_EQ(SimulationSpecFrom(config, settings).workers, Core::kSingleWorker);
}
