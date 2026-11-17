#include "SimulationRuntime.hpp"

#include <gtest/gtest.h>

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <string>
#include <thread>
#include <vector>

#include "MetricsRegistry.hpp"

using namespace Runic;
using namespace Runic::Simulation;
using namespace std::chrono_literals;

namespace {

constexpr std::array<ScheduleSlot, kScheduleSlotCount> kEverySlot{
    ScheduleSlot::Hz20Producers, ScheduleSlot::Hz10Producers, ScheduleSlot::Hz30Realtime,
    ScheduleSlot::Hz20Consumers, ScheduleSlot::Hz10Consumers, ScheduleSlot::Hz60Hook};

// The names of the slots that ran in each of the next `ticks` master ticks.
std::vector<std::vector<std::string>> SlotsPerTick(SimulationRuntime &runtime,
                                                   std::uint64_t ticks) {
    std::vector<std::string> ran;
    for (const ScheduleSlot slot : kEverySlot) {
        runtime.AddSystem({"engine:test_probe", slot, [&ran, slot](const TickContext &) {
                               ran.emplace_back(ScheduleSlotName(slot));
                           }});
    }
    std::vector<std::vector<std::string>> perTick;
    for (std::uint64_t tick = 0; tick < ticks; ++tick) {
        ran.clear();
        runtime.RunMasterTick();
        perTick.push_back(ran);
    }
    return perTick;
}

} // namespace

TEST(SimulationRuntime, AMasterTickRunsTheDueSlotsInOrder) {
    SimulationRuntime runtime;
    const auto ran = SlotsPerTick(runtime, 7);

    using Names = std::vector<std::string>;
    // Tick 0: every domain is due. Producers, the realtime phase, consumers, then the hook.
    EXPECT_EQ(ran[0], (Names{"hz20 producers", "hz10 producers", "hz30 realtime", "hz20 consumers",
                             "hz10 consumers", "hz60 hook"}));
    EXPECT_EQ(ran[1], (Names{"hz60 hook"}));
    EXPECT_EQ(ran[2], (Names{"hz30 realtime", "hz60 hook"}));
    EXPECT_EQ(ran[3], (Names{"hz20 producers", "hz20 consumers", "hz60 hook"}));
    EXPECT_EQ(ran[4], (Names{"hz30 realtime", "hz60 hook"}));
    EXPECT_EQ(ran[5], (Names{"hz60 hook"}));
    EXPECT_EQ(ran[6], ran[0]); // the pattern repeats every six master ticks
}

TEST(SimulationRuntime, SystemsOfASlotRunInTheOrderTheyWereAdded) {
    SimulationRuntime runtime;
    std::vector<int> order;
    for (int system = 0; system < 5; ++system) {
        runtime.AddSystem({"engine:test_ordered", ScheduleSlot::Hz30Realtime,
                           [&order, system](const TickContext &) { order.push_back(system); }});
    }
    runtime.RunMasterTick();
    EXPECT_EQ(order, (std::vector<int>{0, 1, 2, 3, 4}));
}

TEST(SimulationRuntime, ASystemGetsItsDomainsTickStepAndExactTime) {
    SimulationRuntime runtime;
    std::vector<TickContext> seen;
    runtime.AddSystem({"engine:test_slow", ScheduleSlot::Hz10Consumers,
                       [&seen](const TickContext &context) { seen.push_back(context); }});
    for (int tick = 0; tick < 13; ++tick) {
        runtime.RunMasterTick();
    }

    ASSERT_EQ(seen.size(), 3u); // master ticks 0, 6 and 12
    for (std::uint64_t index = 0; index < seen.size(); ++index) {
        EXPECT_EQ(seen[index].domain, TickDomain::Hz10);
        EXPECT_EQ(seen[index].masterTick, Core::TickId(index * 6));
        EXPECT_EQ(seen[index].domainTick, index);
        EXPECT_FLOAT_EQ(seen[index].dt, 0.1f);
        EXPECT_EQ(seen[index].time, Core::Duration(100ms) * static_cast<int>(index));
    }
    EXPECT_EQ(runtime.NextMasterTick(), Core::TickId(13));
    EXPECT_EQ(runtime.Time(), Core::TickRate::Hz(60).TimeOfTick(13));
}

TEST(SimulationRuntime, DomainTickCountsStayExactOverALongRun) {
    SimulationRuntime runtime;

    // Per domain: how many ticks ran, and whether each was the next number at its exact time.
    struct Seen {
        std::uint64_t ticks = 0;
        bool exact = true;
    };
    std::array<Seen, kTickDomainCount> seen;
    for (const ScheduleSlot slot : kEverySlot) {
        runtime.AddSystem(
            {"engine:test_counter", slot, [&seen, slot](const TickContext &context) {
                 Seen &domain = seen[IndexOf(context.domain)];
                 // Hz20 and Hz10 have two slots each: count the tick once.
                 if (slot == ScheduleSlot::Hz20Consumers || slot == ScheduleSlot::Hz10Consumers) {
                     return;
                 }
                 domain.exact =
                     domain.exact && context.domainTick == domain.ticks &&
                     context.time == RateOf(context.domain).TimeOfTick(context.domainTick) &&
                     context.time ==
                         RateOf(TickDomain::Hz60).TimeOfTick(context.masterTick.Value());
                 ++domain.ticks;
             }});
    }

    // Ten hours of simulation and a few ticks more, so the run does not end on a round number.
    constexpr std::uint64_t kMasterTicks = 10 * 60 * 60 * 60 + 7;
    for (std::uint64_t tick = 0; tick < kMasterTicks; ++tick) {
        runtime.RunMasterTick();
    }

    // After n master ticks (0 to n - 1) a domain that ticks every k-th has run ⌈n / k⌉ ticks.
    EXPECT_EQ(runtime.TicksRun(TickDomain::Hz60), kMasterTicks);
    EXPECT_EQ(runtime.TicksRun(TickDomain::Hz30), (kMasterTicks + 1) / 2);
    EXPECT_EQ(runtime.TicksRun(TickDomain::Hz20), (kMasterTicks + 2) / 3);
    EXPECT_EQ(runtime.TicksRun(TickDomain::Hz10), (kMasterTicks + 5) / 6);
    EXPECT_EQ(runtime.TicksRun(TickDomain::Hz30), 1'080'004u);
    EXPECT_EQ(runtime.TicksRun(TickDomain::Hz20), 720'003u);
    EXPECT_EQ(runtime.TicksRun(TickDomain::Hz10), 360'002u);
    for (const TickDomain domain : kTickDomains) {
        EXPECT_EQ(seen[IndexOf(domain)].ticks, runtime.TicksRun(domain)) << TickDomainName(domain);
        EXPECT_TRUE(seen[IndexOf(domain)].exact) << TickDomainName(domain);
    }
    // No drift: ten hours of ticks are ten hours of simulation time, to the nanosecond.
    EXPECT_EQ(Core::TickRate::Hz(60).TimeOfTick(kMasterTicks - 7), Core::Duration(10h));
    EXPECT_EQ(runtime.Time(), Core::Duration(10h) + Core::TickRate::Hz(60).TimeOfTick(7));
}

TEST(SimulationRuntime, ASystemAddedBetweenStepsRunsFromTheNextStep) {
    SimulationRuntime runtime;
    runtime.RunMasterTick();
    runtime.RunMasterTick();

    std::vector<std::uint64_t> ticks;
    runtime.AddSystem(
        {"engine:test_late", ScheduleSlot::Hz60Hook,
         [&ticks](const TickContext &context) { ticks.push_back(context.masterTick.Value()); }});
    runtime.RunMasterTick();
    runtime.RunMasterTick();
    EXPECT_EQ(ticks, (std::vector<std::uint64_t>{2, 3}));
}

TEST(SimulationRuntime, RecordsTicksAndTimePerDomainAndPerSystem) {
    Core::MetricsRegistry metrics;
    SimulationSpec spec;
    spec.metrics = &metrics;
    SimulationRuntime runtime(spec);
    EXPECT_EQ(runtime.Metrics(), &metrics);

    runtime.AddSystem({"engine:test_realtime", ScheduleSlot::Hz30Realtime,
                       [](const TickContext &) { std::this_thread::sleep_for(200us); }});
    runtime.AddSystem(
        {"engine:test_scan", ScheduleSlot::Hz10Producers, [](const TickContext &) {}});

    constexpr std::uint64_t kOneSecond = 60;
    for (std::uint64_t tick = 0; tick < kOneSecond; ++tick) {
        runtime.RunMasterTick();
    }

    // One second of the master clock: 60, 30, 20 and 10 domain ticks.
    const std::uint64_t expected[] = {60, 30, 20, 10};
    for (const TickDomain domain : kTickDomains) {
        const std::string_view label = TickDomainName(domain);
        const Core::CounterId ticks = metrics.AddCounter({kTicksMetric, label, ""});
        const Core::TimingId time = metrics.AddTiming({kTickTimeMetric, label, ""});
        EXPECT_EQ(metrics.Read(ticks), expected[IndexOf(domain)]) << label;
        EXPECT_EQ(metrics.Read(time).count, expected[IndexOf(domain)]) << label;
    }
    const Core::TimingId realtime = metrics.AddTiming({kTickTimeMetric, "hz30", ""});
    EXPECT_GE(metrics.Read(realtime).total, Core::Duration(200us) * 30); // the sleeping system
    const Core::TimingId step = metrics.AddTiming({kStepTimeMetric, Core::kNoMetricDomain, ""});
    EXPECT_EQ(metrics.Read(step).count, kOneSecond);
    EXPECT_GE(metrics.Read(step).total, metrics.Read(realtime).total);

    // Per system, under its name and its domain's label.
    const Core::TimingId system = metrics.AddTiming(
        {std::string(kSystemTimeMetricPrefix) + "engine:test_realtime", "hz30", ""});
    EXPECT_EQ(metrics.Read(system).count, 30u);
    EXPECT_GE(metrics.Read(system).max, Core::Duration(200us));
    const Core::TimingId scan =
        metrics.AddTiming({std::string(kSystemTimeMetricPrefix) + "engine:test_scan", "hz10", ""});
    EXPECT_EQ(metrics.Read(scan).count, 10u);
}

TEST(SimulationRuntime, CountsAStepThatTakesLongerThanItsInterval) {
    Core::MetricsRegistry metrics;
    SimulationSpec spec;
    spec.metrics = &metrics;
    SimulationRuntime runtime(spec);
    bool slow = false;
    runtime.AddSystem({"engine:test_stall", ScheduleSlot::Hz60Hook, [&slow](const TickContext &) {
                           if (slow) {
                               std::this_thread::sleep_for(20ms); // more than 1/60 s
                           }
                       }});
    const Core::CounterId overruns =
        metrics.AddCounter({kOverrunsMetric, Core::kNoMetricDomain, ""});

    runtime.RunMasterTick();
    EXPECT_EQ(metrics.Read(overruns), 0u);
    slow = true;
    runtime.RunMasterTick();
    runtime.RunMasterTick();
    EXPECT_EQ(metrics.Read(overruns), 2u);
    // The steps ran all the same: an overrun is measured, never skipped.
    EXPECT_EQ(runtime.TicksRun(TickDomain::Hz60), 3u);
}

TEST(SimulationRuntime, ASystemSplitsItsWorkAcrossTheSimulationsWorkers) {
    SimulationSpec spec;
    spec.workers = 3;
    spec.id = Core::SimulationId(7);
    SimulationRuntime runtime(spec);
    EXPECT_EQ(runtime.Id(), Core::SimulationId(7));
    EXPECT_EQ(runtime.Workers().WorkerCount(), 3u);

    std::atomic<std::uint64_t> sum{0};
    runtime.AddSystem(
        {"engine:test_parallel", ScheduleSlot::Hz30Realtime, [&](const TickContext &) {
             runtime.Workers().ParallelFor(
                 1'000, 50, [&sum](const Core::ChunkRange &range, Core::WorkerIndex) {
                     for (std::uint32_t index = range.begin; index < range.end; ++index) {
                         sum.fetch_add(index);
                     }
                 });
         }});
    runtime.RunMasterTick();
    EXPECT_EQ(sum.load(), 499'500u); // the system returned after its barrier
}

TEST(SimulationRuntime, AStandaloneSimulationHasAnIdAndOneWorker) {
    SimulationRuntime runtime;
    EXPECT_EQ(runtime.Id(), kStandaloneSimulation);
    EXPECT_TRUE(runtime.Id().IsValid());
    EXPECT_EQ(runtime.Workers().WorkerCount(), Core::kSingleWorker);
    EXPECT_EQ(runtime.Metrics(), nullptr);
    EXPECT_EQ(runtime.NextMasterTick(), Core::TickId(0));
    EXPECT_EQ(runtime.Time(), Core::Duration(0));
    runtime.RunMasterTick(); // no system, no registry: a step still counts
    EXPECT_EQ(runtime.TicksRun(TickDomain::Hz10), 1u);
}

#if RUNIC_ASSERTS_ENABLED

TEST(SimulationRuntimeDeathTest, ASystemIsNotAddedWhileAStepRuns) {
    SimulationRuntime runtime;
    runtime.AddSystem(
        {"engine:test_adder", ScheduleSlot::Hz60Hook, [&runtime](const TickContext &) {
             runtime.AddSystem(
                 {"engine:test_added", ScheduleSlot::Hz60Hook, [](const TickContext &) {}});
         }});
    EXPECT_DEATH(runtime.RunMasterTick(), "added while a master step runs");
}

#endif // RUNIC_ASSERTS_ENABLED
