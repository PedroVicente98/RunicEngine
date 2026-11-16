#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <string_view>

#include "Time.hpp"

// The metrics registry: the counters, gauges and timings a process records, each labelled with the
// clock domain it belongs to (RunicDocs › Tools › Metrics every process exports). Recording is a
// few relaxed atomic operations from any thread. The Platform's exporter reads the registry and
// publishes it; the Engine never talks to a monitoring system. A registry is an object the program
// owns and hands to what records into it, not a global.

namespace Runic::Core {

enum class MetricKind : std::uint8_t {
    Counter, // a count that only grows: ticks run, events flushed
    Gauge,   // the latest value of something that goes up and down: backlog, entities
    Timing,  // how long something took: a count, a total, a maximum and a histogram
};

// What a metric is. A metric is identified by its name and its domain together, so one name may be
// recorded once per clock domain ("simulation.tick" in "hz60", "hz30", "hz20", "hz10").
struct MetricDescription {
    std::string_view name;   // lowercase words joined by dots or underscores
    std::string_view domain; // the clock domain's label; kNoMetricDomain for a process-wide metric
    std::string_view help;   // one line for the exporter
};

inline constexpr std::string_view kNoMetricDomain{};

// Metrics a registry holds unless its constructor is given another capacity.
inline constexpr std::uint32_t kDefaultMetricCapacity = 512;

// A timing's histogram: bucket i counts the samples of at most kFirstTimingBucket × 2^i, and the
// last bucket also takes everything longer (16 µs to about 8.4 s). Percentiles come from it.
inline constexpr std::size_t kTimingBuckets = 20;
inline constexpr Duration kFirstTimingBucket{16'000};

// The longest sample bucket i counts.
constexpr Duration TimingBucketLimit(std::size_t bucket) noexcept {
    return Duration(kFirstTimingBucket.count() << bucket);
}

// The handle of a registered metric. A default handle, or one a full registry gave, is invalid:
// recording through it does nothing.
template <MetricKind Kind> class MetricId {
  public:
    constexpr MetricId() noexcept = default;
    constexpr bool IsValid() const noexcept { return m_Index != kInvalidIndex; }
    constexpr std::uint32_t Index() const noexcept { return m_Index; }

    friend constexpr bool operator==(const MetricId &, const MetricId &) noexcept = default;

  private:
    friend class MetricsRegistry;

    static constexpr std::uint32_t kInvalidIndex = 0xFFFF'FFFF;

    constexpr explicit MetricId(std::uint32_t index) noexcept : m_Index(index) {}

    std::uint32_t m_Index = kInvalidIndex;
};

using CounterId = MetricId<MetricKind::Counter>;
using GaugeId = MetricId<MetricKind::Gauge>;
using TimingId = MetricId<MetricKind::Timing>;

struct TimingSnapshot {
    std::uint64_t count = 0;
    Duration total{};
    Duration max{};
    Duration last{};
    std::array<std::uint64_t, kTimingBuckets> buckets{};

    // The mean sample; zero before the first one.
    Duration Mean() const noexcept;
    // The limit of the bucket that holds the given fraction of the samples (0.5 for p50, 0.99 for
    // p99), never above max: an upper estimate, as exact as the buckets. Zero before the first
    // sample.
    Duration Percentile(double fraction) const noexcept;
};

// One metric as an exporter reads it. The texts stay valid while the registry lives.
struct MetricSnapshot {
    MetricKind kind = MetricKind::Counter;
    std::string_view name;
    std::string_view domain;
    std::string_view help;
    std::uint64_t counter = 0; // for a Counter
    double gauge = 0.0;        // for a Gauge
    TimingSnapshot timing;     // for a Timing
};

class MetricsRegistry {
  public:
    // Allocates every slot once; the registry never allocates on a record.
    explicit MetricsRegistry(std::uint32_t capacity = kDefaultMetricCapacity);
    ~MetricsRegistry();

    MetricsRegistry(const MetricsRegistry &) = delete;
    MetricsRegistry &operator=(const MetricsRegistry &) = delete;

    // Registration, at start: thread-safe, and it may allocate. Registering a name and domain
    // that exist returns the existing metric (its kind must match). A full registry logs an error
    // and returns an invalid handle.
    CounterId AddCounter(const MetricDescription &description);
    GaugeId AddGauge(const MetricDescription &description);
    TimingId AddTiming(const MetricDescription &description);

    // Recording: thread-safe, lock-free, no allocation.
    void Increment(CounterId id) noexcept;
    void Add(CounterId id, std::uint64_t amount) noexcept;
    void Set(GaugeId id, double value) noexcept;
    void Record(TimingId id, Duration sample) noexcept;

    // Reading, for exporters and tests: thread-safe. A snapshot taken while other threads record
    // is not one instant, but each of its values is one that was recorded.
    std::uint32_t Count() const noexcept;
    std::uint32_t Capacity() const noexcept;
    MetricSnapshot Read(std::uint32_t index) const;
    std::uint64_t Read(CounterId id) const noexcept;
    double Read(GaugeId id) const noexcept;
    TimingSnapshot Read(TimingId id) const noexcept;

  private:
    struct Slot;
    struct State;

    std::uint32_t Register(MetricKind kind, const MetricDescription &description);
    Slot *SlotAt(std::uint32_t index) const noexcept;

    std::unique_ptr<State> m_State;
};

// Records the time from its construction to its destruction as one sample.
class ScopedTiming {
  public:
    ScopedTiming(MetricsRegistry &registry, TimingId id) noexcept
        : m_Registry(registry), m_Id(id), m_Start(MonotonicNow()) {}
    ~ScopedTiming() { m_Registry.Record(m_Id, MonotonicNow() - m_Start); }

    ScopedTiming(const ScopedTiming &) = delete;
    ScopedTiming &operator=(const ScopedTiming &) = delete;

  private:
    MetricsRegistry &m_Registry;
    TimingId m_Id;
    TimePoint m_Start;
};

} // namespace Runic::Core
