#include "MetricsRegistry.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <string>
#include <thread>
#include <vector>

#include "AllocationCounter.hpp"
#include "Log.hpp"

using namespace Runic::Core;
using namespace std::chrono_literals;

namespace {

// The labels a simulation gives its clock domains; Core only stores them.
constexpr std::string_view kHz60 = "hz60";
constexpr std::string_view kHz30 = "hz30";
constexpr std::string_view kHz20 = "hz20";
constexpr std::string_view kHz10 = "hz10";

} // namespace

TEST(MetricsRegistry, RecordsCountersGaugesAndTimings) {
    MetricsRegistry metrics;
    const CounterId ticks = metrics.AddCounter({"test.ticks", kNoMetricDomain, "Ticks run"});
    const GaugeId backlog = metrics.AddGauge({"test.backlog", kNoMetricDomain, "Ticks owed"});
    const TimingId tick = metrics.AddTiming({"test.tick", kNoMetricDomain, "Time per tick"});
    ASSERT_TRUE(ticks.IsValid());
    ASSERT_TRUE(backlog.IsValid());
    ASSERT_TRUE(tick.IsValid());
    EXPECT_EQ(metrics.Count(), 3u);

    metrics.Increment(ticks);
    metrics.Add(ticks, 4);
    metrics.Set(backlog, 2.0);
    metrics.Set(backlog, 0.5);
    metrics.Record(tick, 2ms);
    metrics.Record(tick, 6ms);
    metrics.Record(tick, 1ms);

    EXPECT_EQ(metrics.Read(ticks), 5u);
    EXPECT_DOUBLE_EQ(metrics.Read(backlog), 0.5); // a gauge keeps the latest value
    const TimingSnapshot timing = metrics.Read(tick);
    EXPECT_EQ(timing.count, 3u);
    EXPECT_EQ(timing.total, Duration(9ms));
    EXPECT_EQ(timing.max, Duration(6ms));
    EXPECT_EQ(timing.last, Duration(1ms));
    EXPECT_EQ(timing.Mean(), Duration(3ms));
}

TEST(MetricsRegistry, KeepsOneMetricPerClockDomain) {
    MetricsRegistry metrics;
    const std::string_view domains[] = {kHz60, kHz30, kHz20, kHz10};
    std::vector<TimingId> time;
    std::vector<CounterId> ticks;
    for (const std::string_view domain : domains) {
        time.push_back(metrics.AddTiming({"simulation.tick", domain, "Time per domain tick"}));
        ticks.push_back(metrics.AddCounter({"simulation.ticks", domain, "Domain ticks run"}));
    }

    // One second of a 60 Hz master clock: 60, 30, 20 and 10 domain ticks, each with its own time.
    const std::uint64_t perSecond[] = {60, 30, 20, 10};
    for (std::size_t domain = 0; domain < std::size(domains); ++domain) {
        for (std::uint64_t tick = 0; tick < perSecond[domain]; ++tick) {
            metrics.Record(time[domain], Duration(100us) * static_cast<int>(domain + 1));
            metrics.Increment(ticks[domain]);
        }
    }

    for (std::size_t domain = 0; domain < std::size(domains); ++domain) {
        EXPECT_EQ(metrics.Read(ticks[domain]), perSecond[domain]) << domains[domain];
        const TimingSnapshot timing = metrics.Read(time[domain]);
        EXPECT_EQ(timing.count, perSecond[domain]) << domains[domain];
        EXPECT_EQ(timing.max, Duration(100us) * static_cast<int>(domain + 1)) << domains[domain];
    }

    // An exporter walks the registry and finds every metric with its domain label.
    std::uint32_t hz30Metrics = 0;
    for (std::uint32_t index = 0; index < metrics.Count(); ++index) {
        const MetricSnapshot snapshot = metrics.Read(index);
        if (snapshot.domain == kHz30) {
            ++hz30Metrics;
            if (snapshot.kind == MetricKind::Timing) {
                EXPECT_EQ(snapshot.name, "simulation.tick");
                EXPECT_EQ(snapshot.help, "Time per domain tick");
                EXPECT_EQ(snapshot.timing.count, 30u);
            } else {
                EXPECT_EQ(snapshot.kind, MetricKind::Counter);
                EXPECT_EQ(snapshot.counter, 30u);
            }
        }
    }
    EXPECT_EQ(hz30Metrics, 2u);
}

TEST(MetricsRegistry, RegisteringTheSameMetricAgainGivesTheSameHandle) {
    MetricsRegistry metrics;
    const CounterId first = metrics.AddCounter({"test.events", kHz30, "Events"});
    const CounterId again = metrics.AddCounter({"test.events", kHz30, "Events"});
    const CounterId other = metrics.AddCounter({"test.events", kHz10, "Events"});
    EXPECT_EQ(first, again);
    EXPECT_NE(first, other);
    EXPECT_EQ(metrics.Count(), 2u);
}

TEST(MetricsRegistry, TheDescriptionIsCopied) {
    MetricsRegistry metrics;
    {
        const std::string name = "test.copied";
        const std::string domain = "hz20";
        const std::string help = "Kept after the caller's strings are gone";
        metrics.AddGauge({name, domain, help});
    }
    const MetricSnapshot snapshot = metrics.Read(0);
    EXPECT_EQ(snapshot.kind, MetricKind::Gauge);
    EXPECT_EQ(snapshot.name, "test.copied");
    EXPECT_EQ(snapshot.domain, "hz20");
    EXPECT_EQ(snapshot.help, "Kept after the caller's strings are gone");
}

TEST(MetricsRegistry, TimingBucketsGivePercentiles) {
    MetricsRegistry metrics;
    const TimingId tick = metrics.AddTiming({"test.tick", kHz30, "Time per tick"});

    // 90 fast ticks, 9 slower and one very slow.
    for (int sample = 0; sample < 90; ++sample) {
        metrics.Record(tick, 100us);
    }
    for (int sample = 0; sample < 9; ++sample) {
        metrics.Record(tick, 3ms);
    }
    metrics.Record(tick, 40ms);

    const TimingSnapshot timing = metrics.Read(tick);
    EXPECT_EQ(timing.count, 100u);
    // 100 µs falls in the bucket of at most 128 µs, 3 ms in 4.096 ms, and 40 ms in 65.536 ms.
    EXPECT_EQ(timing.Percentile(0.50), Duration(128us));
    EXPECT_EQ(timing.Percentile(0.95), Duration(4096us));
    EXPECT_EQ(timing.Percentile(0.99), Duration(4096us));
    EXPECT_EQ(timing.Percentile(1.00), Duration(40ms)); // never above the maximum
    EXPECT_EQ(TimingBucketLimit(0), kFirstTimingBucket);
    EXPECT_EQ(TimingBucketLimit(3), Duration(128us));

    std::uint64_t counted = 0;
    for (const std::uint64_t bucket : timing.buckets) {
        counted += bucket;
    }
    EXPECT_EQ(counted, 100u);

    const TimingSnapshot empty = metrics.Read(metrics.AddTiming({"test.unused", kHz30, ""}));
    EXPECT_EQ(empty.Percentile(0.99), Duration(0));
    EXPECT_EQ(empty.Mean(), Duration(0));
}

TEST(MetricsRegistry, ASampleLongerThanEveryBucketCountsInTheLast) {
    MetricsRegistry metrics;
    const TimingId tick = metrics.AddTiming({"test.tick", kNoMetricDomain, ""});
    metrics.Record(tick, std::chrono::seconds(60));
    metrics.Record(tick, Duration(-5)); // a clock that went back counts as no time
    const TimingSnapshot timing = metrics.Read(tick);
    EXPECT_EQ(timing.buckets[kTimingBuckets - 1], 1u);
    EXPECT_EQ(timing.buckets[0], 1u);
    EXPECT_EQ(timing.max, Duration(std::chrono::seconds(60)));
    EXPECT_EQ(timing.last, Duration(0));
}

TEST(MetricsRegistry, RecordingDoesNotAllocate) {
    MetricsRegistry metrics;
    const CounterId ticks = metrics.AddCounter({"test.ticks", kHz60, ""});
    const GaugeId backlog = metrics.AddGauge({"test.backlog", kHz60, ""});
    const TimingId tick = metrics.AddTiming({"test.tick", kHz60, ""});

    std::size_t allocations = 0;
    {
        Testing::AllocationScope scope;
        for (int round = 0; round < 10'000; ++round) {
            metrics.Increment(ticks);
            metrics.Set(backlog, static_cast<double>(round));
            metrics.Record(tick, Duration(round));
            { const ScopedTiming timed(metrics, tick); }
        }
        allocations = scope.Count();
    }
    EXPECT_EQ(allocations, 0u);
    EXPECT_EQ(metrics.Read(ticks), 10'000u);
    EXPECT_EQ(metrics.Read(tick).count, 20'000u);
}

TEST(MetricsRegistry, CountsFromSeveralThreadsAddUp) {
    MetricsRegistry metrics;
    const CounterId events = metrics.AddCounter({"test.events", kHz30, ""});
    const TimingId time = metrics.AddTiming({"test.time", kHz30, ""});

    constexpr int kThreads = 4;
    constexpr int kPerThread = 50'000;
    std::vector<std::thread> threads;
    for (int thread = 0; thread < kThreads; ++thread) {
        threads.emplace_back([&metrics, events, time, thread] {
            for (int round = 0; round < kPerThread; ++round) {
                metrics.Increment(events);
                metrics.Record(time, Duration(1'000 * (thread + 1)));
            }
        });
    }
    for (std::thread &thread : threads) {
        thread.join();
    }

    EXPECT_EQ(metrics.Read(events), static_cast<std::uint64_t>(kThreads) * kPerThread);
    const TimingSnapshot timing = metrics.Read(time);
    EXPECT_EQ(timing.count, static_cast<std::uint64_t>(kThreads) * kPerThread);
    EXPECT_EQ(timing.total, Duration(1'000) * (1 + 2 + 3 + 4) * kPerThread);
    EXPECT_EQ(timing.max, Duration(4'000));
}

TEST(MetricsRegistry, AFullRegistryRefusesTheMetricAndIgnoresItsRecords) {
    struct Captured {
        std::vector<std::string> errors;
    } captured;
    SetLogSink(
        [](const LogRecord &record, void *user) {
            if (record.level == LogLevel::Error) {
                static_cast<Captured *>(user)->errors.emplace_back(record.message);
            }
        },
        &captured);

    MetricsRegistry metrics(2);
    EXPECT_EQ(metrics.Capacity(), 2u);
    const CounterId first = metrics.AddCounter({"test.first", kNoMetricDomain, ""});
    const CounterId second = metrics.AddCounter({"test.second", kNoMetricDomain, ""});
    const CounterId third = metrics.AddCounter({"test.third", kNoMetricDomain, ""});
    const TimingId fourth = metrics.AddTiming({"test.fourth", kNoMetricDomain, ""});
    SetLogSink(nullptr, nullptr);

    EXPECT_TRUE(first.IsValid());
    EXPECT_TRUE(second.IsValid());
    EXPECT_FALSE(third.IsValid());
    EXPECT_FALSE(fourth.IsValid());
    ASSERT_EQ(captured.errors.size(), 1u); // reported once
    EXPECT_NE(captured.errors[0].find("test.third"), std::string::npos);

    metrics.Increment(third); // ignored
    metrics.Record(fourth, 1ms);
    metrics.Increment(CounterId()); // a default handle too
    EXPECT_EQ(metrics.Read(third), 0u);
    EXPECT_EQ(metrics.Read(fourth).count, 0u);
    EXPECT_EQ(metrics.Count(), 2u);
}

TEST(MetricsRegistry, ScopedTimingRecordsItsScope) {
    MetricsRegistry metrics;
    const TimingId scope = metrics.AddTiming({"test.scope", kNoMetricDomain, ""});
    {
        const ScopedTiming timed(metrics, scope);
        std::this_thread::sleep_for(2ms);
    }
    const TimingSnapshot timing = metrics.Read(scope);
    EXPECT_EQ(timing.count, 1u);
    EXPECT_GE(timing.last, Duration(2ms));
    EXPECT_LT(timing.last, Duration(std::chrono::seconds(5)));
}
