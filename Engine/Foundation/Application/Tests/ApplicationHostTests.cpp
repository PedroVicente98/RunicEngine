#include "AllocationCounter.hpp"
#include "ApplicationHost.hpp"
#include "TestLayers.hpp"

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

using namespace Runic::Application;
using Runic::Application::Testing::CountingLayer;
using Runic::Application::Testing::Journal;
using Runic::Application::Testing::RecordingLayer;
using Runic::Application::Testing::TestClock;
using Runic::Core::Duration;
using Lines = std::vector<std::string>;

namespace {

// An application that remembers how it stopped.
class TestApplication final : public Application {
  public:
    using Application::Application;
    int stoppedWith = -1;
    std::uint64_t stoppedAfter = 0;
    void OnStopped(int exitCode, std::uint64_t frames) override {
        stoppedWith = exitCode;
        stoppedAfter = frames;
    }
};

struct Rig {
    Journal journal;
    TestClock::Record clock;
    TestApplication *app = nullptr;
    std::unique_ptr<ApplicationHost> host;

    explicit Rig(ApplicationSpec spec, Duration step = Duration(1'000'000)) {
        auto application = std::make_unique<TestApplication>(std::move(spec));
        app = application.get();
        host = std::make_unique<ApplicationHost>(std::move(application), CommandLine({"test"}),
                                                 std::make_unique<TestClock>(clock, step));
    }
    RecordingLayer &Push(const std::string &name) {
        auto layer = std::make_unique<RecordingLayer>(name, journal);
        RecordingLayer &ref = *layer;
        app->PushLayer(std::move(layer));
        return ref;
    }
};

ApplicationSpec Spec(LoopPolicy loop = LoopPolicy::Fixed(60), std::uint64_t maxFrames = 0) {
    return ApplicationSpec{"test", loop, maxFrames};
}

} // namespace

TEST(ApplicationHost, RunFrameIsOneCompleteFrame) {
    Rig rig(Spec());
    rig.Push("A");
    rig.Push("B");
    ASSERT_TRUE(rig.host->Start());
    rig.host->Context().PostEvent(AppEvent{KeyEvent{65, 0, true, false}});
    rig.journal.Clear();
    rig.host->RunFrame(FrameTime{});
    EXPECT_EQ(rig.journal.lines, (Lines{"B event Key", "A event Key", "A update", "B update",
                                        "A render", "B render"}));
    EXPECT_EQ(rig.host->FramesRun(), 1u);
}

TEST(ApplicationHost, AnEventPostedDuringAFrameWaitsForTheNext) {
    Rig rig(Spec());
    RecordingLayer &a = rig.Push("A");
    bool posted = false;
    a.onUpdate = [&posted](LayerContext &context, const FrameTime &) {
        if (!posted) {
            context.PostEvent(AppEvent{WindowFocusEvent{true}});
            posted = true;
        }
    };
    ASSERT_TRUE(rig.host->Start());
    rig.journal.Clear();
    rig.host->RunFrame(FrameTime{});
    EXPECT_EQ(rig.journal.lines, (Lines{"A update", "A render"}));
    rig.journal.Clear();
    rig.host->RunFrame(FrameTime{});
    EXPECT_EQ(rig.journal.lines, (Lines{"A event WindowFocus", "A update", "A render"}));
}

TEST(ApplicationHost, PostingAndDispatchingEventsNeverAllocates) {
    auto application = std::make_unique<Application>(Spec());
    auto bottom = std::make_unique<CountingLayer>("bottom");
    auto top = std::make_unique<CountingLayer>("top");
    CountingLayer &counted = *bottom;
    application->PushLayer(std::move(bottom));
    application->PushLayer(std::move(top));
    ApplicationHost host(std::move(application), CommandLine({"test"}));
    ASSERT_TRUE(host.Start());

    std::size_t allocations = 0;
    {
        Testing::AllocationScope scope;
        for (int frame = 0; frame < 100; ++frame) {
            for (int i = 0; i < 50; ++i) {
                host.Context().PostEvent(AppEvent{MouseMoveEvent{1.0f, 2.0f, 0.5f, 0.5f}});
            }
            host.RunFrame(FrameTime{});
        }
        allocations = scope.Count();
    }
    EXPECT_EQ(allocations, 0u);
    EXPECT_EQ(counted.events, 5'000u);
    EXPECT_EQ(counted.updates, 100u);
}

TEST(ApplicationHost, TheFixedLoopKeepsAnExactSchedule) {
    Rig rig(Spec(LoopPolicy::Fixed(60), 120));
    RecordingLayer &a = rig.Push("A");
    std::vector<FrameTime> frames;
    a.onUpdate = [&frames](LayerContext &, const FrameTime &time) { frames.push_back(time); };

    EXPECT_EQ(rig.host->Run(), 0);
    ASSERT_EQ(frames.size(), 120u);
    ASSERT_EQ(rig.clock.sleeps.size(), 120u);
    const Runic::Core::TickRate rate = Runic::Core::TickRate::Hz(60);
    Duration sum{0};
    for (std::uint64_t n = 0; n < 120; ++n) {
        ASSERT_EQ(frames[n].frame, n);
        ASSERT_EQ(frames[n].sinceStart, rate.TimeOfTick(n));
        sum += frames[n].delta;
    }
    EXPECT_EQ(frames[0].delta, Duration(0));
    EXPECT_EQ(frames[1].delta, Duration(16'666'667));
    EXPECT_EQ(frames[3].sinceStart, Duration(50'000'000));
    EXPECT_EQ(sum, frames[119].sinceStart); // the deltas add up exactly: no drift
    EXPECT_EQ(rig.app->stoppedWith, 0);
    EXPECT_EQ(rig.app->stoppedAfter, 120u);
}

TEST(ApplicationHost, TheTopLayerPacesTheOtherLoop) {
    Rig rig(Spec(LoopPolicy::PacedByTopLayer(), 5), Duration(2'000'000));
    RecordingLayer &a = rig.Push("A");
    std::vector<FrameTime> frames;
    a.onUpdate = [&frames](LayerContext &, const FrameTime &time) { frames.push_back(time); };
    EXPECT_EQ(rig.host->Run(), 0);
    EXPECT_TRUE(rig.clock.sleeps.empty());
    ASSERT_EQ(frames.size(), 5u);
    EXPECT_EQ(frames[0].delta, Duration(0));
    EXPECT_EQ(frames[1].delta, Duration(2'000'000));
    EXPECT_EQ(frames[4].sinceStart,
              Duration(10'000'000)); // Now() advanced 2 ms per frame after the start
}

TEST(ApplicationHost, AStopTakesEffectAfterTheFramesSafePoint) {
    Rig rig(Spec(LoopPolicy::Fixed(60)));
    RecordingLayer &a = rig.Push("A");
    a.onUpdate = [&rig](LayerContext &context, const FrameTime &time) {
        if (time.frame == 2) {
            context.PushLayer(std::make_unique<RecordingLayer>("Late", rig.journal));
            context.RequestStop(0);
        }
    };
    rig.journal.Clear();
    EXPECT_EQ(rig.host->Run(), 0);
    EXPECT_EQ(rig.host->FramesRun(), 3u);
    const Lines tail(rig.journal.lines.end() - 5, rig.journal.lines.end());
    EXPECT_EQ(tail, (Lines{"A update", "A render", "Late attach", "Late detach", "A detach"}));
    EXPECT_EQ(rig.app->stoppedAfter, 3u);
}

TEST(ApplicationHost, AFailedStartStopsWithExitCodeOne) {
    Rig rig(Spec());
    rig.Push("A");
    rig.Push("B").refuseAttach = true;
    rig.Push("C");
    EXPECT_EQ(rig.host->Run(), 1);
    EXPECT_EQ(rig.host->FramesRun(), 0u);
    EXPECT_EQ(rig.journal.lines,
              (Lines{"A attach", "B attach", "C attach", "C detach", "A detach"}));
    EXPECT_EQ(rig.app->stoppedWith, 1);
}

TEST(ApplicationHost, AnUnhandledQuitRequestStops) {
    Rig rig(Spec());
    rig.Push("A");
    ASSERT_TRUE(rig.host->Start());
    rig.host->Context().PostEvent(AppEvent{QuitRequestEvent{}});
    rig.host->RunFrame(FrameTime{});
    EXPECT_TRUE(rig.host->StopRequested());
    EXPECT_EQ(rig.host->ExitCode(), 0);
}

TEST(ApplicationHost, TheFirstStopRequestDecidesTheExitCode) {
    Rig rig(Spec());
    rig.host->RequestStop(3);
    rig.host->RequestStop(0);
    EXPECT_EQ(rig.host->ExitCode(), 3);
}

TEST(ApplicationHost, AFullEventQueueDropsAndCounts) {
    Rig rig(Spec());
    rig.Push("A");
    ASSERT_TRUE(rig.host->Start());
    for (std::size_t i = 0; i < ApplicationHost::kEventQueueCapacity + 44; ++i) {
        rig.host->Context().PostEvent(AppEvent{WindowFocusEvent{true}});
    }
    EXPECT_EQ(rig.host->DroppedEvents(), 44u);
}
