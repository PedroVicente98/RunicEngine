// The metrics registry (MetricsRegistry.hpp): a fixed array of slots, registered under a mutex and
// recorded into with relaxed atomics.

#include "MetricsRegistry.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <mutex>
#include <string>

#include "Assert.hpp"
#include "Log.hpp"

namespace Runic::Core {

namespace {

constexpr std::uint64_t kNoSamples = 0;
constexpr Duration kNoTime{0};

// The index a default handle holds: no slot.
constexpr std::uint32_t kInvalidIndex = CounterId().Index();

// The bucket that counts a sample: the first whose limit is at least the sample.
std::size_t BucketOf(Duration sample) noexcept {
    if (sample <= kFirstTimingBucket) {
        return 0;
    }
    // ⌈sample / first⌉ rounded up to a power of two gives the bucket's limit in units of first.
    const auto units = static_cast<std::uint64_t>(
        (sample.count() + kFirstTimingBucket.count() - 1) / kFirstTimingBucket.count());
    const auto bucket = static_cast<std::size_t>(std::bit_width(units - 1));
    return std::min(bucket, kTimingBuckets - 1);
}

} // namespace

Duration TimingSnapshot::Mean() const noexcept {
    return count == kNoSamples ? kNoTime : total / static_cast<Duration::rep>(count);
}

Duration TimingSnapshot::Percentile(double fraction) const noexcept {
    if (count == kNoSamples) {
        return kNoTime;
    }
    const double clamped = std::clamp(fraction, 0.0, 1.0);
    // The rank of the sample asked for, from 1 to count.
    const auto rank = std::max<std::uint64_t>(
        1, static_cast<std::uint64_t>(std::ceil(clamped * static_cast<double>(count))));
    std::uint64_t seen = 0;
    for (std::size_t bucket = 0; bucket < kTimingBuckets; ++bucket) {
        seen += buckets[bucket];
        if (seen >= rank) {
            return std::min(TimingBucketLimit(bucket), max);
        }
    }
    return max;
}

// One metric. The description is written once, before the slot is published through the count;
// the values are atomics every thread may touch.
struct MetricsRegistry::Slot {
    MetricKind kind = MetricKind::Counter;
    std::string name;
    std::string domain;
    std::string help;

    std::atomic<std::uint64_t> counter{0};
    std::atomic<double> gauge{0.0};

    std::atomic<std::uint64_t> timingCount{kNoSamples};
    std::atomic<Duration::rep> timingTotal{0};
    std::atomic<Duration::rep> timingMax{0};
    std::atomic<Duration::rep> timingLast{0};
    std::array<std::atomic<std::uint64_t>, kTimingBuckets> timingBuckets{};
};

struct MetricsRegistry::State {
    explicit State(std::uint32_t capacity)
        : slots(std::make_unique<Slot[]>(capacity)), capacity(capacity) {}

    std::unique_ptr<Slot[]> slots;
    const std::uint32_t capacity;
    std::atomic<std::uint32_t> count{0}; // slots [0, count) are registered and never change again
    std::mutex registration;
    bool reportedFull = false;
};

MetricsRegistry::MetricsRegistry(std::uint32_t capacity)
    : m_State(std::make_unique<State>(capacity)) {
    RUNIC_ASSERT(capacity > 0);
}

MetricsRegistry::~MetricsRegistry() = default;

std::uint32_t MetricsRegistry::Register(MetricKind kind, const MetricDescription &description) {
    RUNIC_ASSERT(!description.name.empty());
    const std::lock_guard lock(m_State->registration);
    const std::uint32_t count = m_State->count.load(std::memory_order_relaxed);
    for (std::uint32_t index = 0; index < count; ++index) {
        const Slot &slot = m_State->slots[index];
        if (slot.name == description.name && slot.domain == description.domain) {
            RUNIC_ASSERT_MSG(slot.kind == kind, "a metric was registered again as another kind");
            return slot.kind == kind ? index : kInvalidIndex;
        }
    }
    if (count == m_State->capacity) {
        if (!m_State->reportedFull) {
            m_State->reportedFull = true;
            RUNIC_LOG_ERROR("RUNIC: the metrics registry is full ({} metrics); '{}' and later "
                            "metrics are not recorded",
                            m_State->capacity, description.name);
        }
        return kInvalidIndex;
    }
    Slot &slot = m_State->slots[count];
    slot.kind = kind;
    slot.name = description.name;
    slot.domain = description.domain;
    slot.help = description.help;
    m_State->count.store(count + 1, std::memory_order_release);
    return count;
}

CounterId MetricsRegistry::AddCounter(const MetricDescription &description) {
    return CounterId(Register(MetricKind::Counter, description));
}

GaugeId MetricsRegistry::AddGauge(const MetricDescription &description) {
    return GaugeId(Register(MetricKind::Gauge, description));
}

TimingId MetricsRegistry::AddTiming(const MetricDescription &description) {
    return TimingId(Register(MetricKind::Timing, description));
}

MetricsRegistry::Slot *MetricsRegistry::SlotAt(std::uint32_t index) const noexcept {
    return index < m_State->count.load(std::memory_order_acquire) ? &m_State->slots[index]
                                                                  : nullptr;
}

void MetricsRegistry::Increment(CounterId id) noexcept {
    constexpr std::uint64_t kOneEvent = 1;
    Add(id, kOneEvent);
}

void MetricsRegistry::Add(CounterId id, std::uint64_t amount) noexcept {
    if (Slot *slot = SlotAt(id.Index())) {
        slot->counter.fetch_add(amount, std::memory_order_relaxed);
    }
}

void MetricsRegistry::Set(GaugeId id, double value) noexcept {
    if (Slot *slot = SlotAt(id.Index())) {
        slot->gauge.store(value, std::memory_order_relaxed);
    }
}

void MetricsRegistry::Record(TimingId id, Duration sample) noexcept {
    Slot *found = SlotAt(id.Index());
    if (found == nullptr) {
        return;
    }
    Slot &slot = *found;
    const Duration::rep nanoseconds = std::max(sample, kNoTime).count();
    slot.timingCount.fetch_add(1, std::memory_order_relaxed);
    slot.timingTotal.fetch_add(nanoseconds, std::memory_order_relaxed);
    slot.timingLast.store(nanoseconds, std::memory_order_relaxed);
    Duration::rep max = slot.timingMax.load(std::memory_order_relaxed);
    while (nanoseconds > max &&
           !slot.timingMax.compare_exchange_weak(max, nanoseconds, std::memory_order_relaxed)) {
    }
    slot.timingBuckets[BucketOf(Duration(nanoseconds))].fetch_add(1, std::memory_order_relaxed);
}

std::uint32_t MetricsRegistry::Count() const noexcept {
    return m_State->count.load(std::memory_order_acquire);
}

std::uint32_t MetricsRegistry::Capacity() const noexcept { return m_State->capacity; }

std::uint64_t MetricsRegistry::Read(CounterId id) const noexcept {
    const Slot *slot = SlotAt(id.Index());
    return slot != nullptr ? slot->counter.load(std::memory_order_relaxed) : 0;
}

double MetricsRegistry::Read(GaugeId id) const noexcept {
    const Slot *slot = SlotAt(id.Index());
    return slot != nullptr ? slot->gauge.load(std::memory_order_relaxed) : 0.0;
}

TimingSnapshot MetricsRegistry::Read(TimingId id) const noexcept {
    TimingSnapshot snapshot;
    const Slot *slot = SlotAt(id.Index());
    if (slot == nullptr) {
        return snapshot;
    }
    snapshot.count = slot->timingCount.load(std::memory_order_relaxed);
    snapshot.total = Duration(slot->timingTotal.load(std::memory_order_relaxed));
    snapshot.max = Duration(slot->timingMax.load(std::memory_order_relaxed));
    snapshot.last = Duration(slot->timingLast.load(std::memory_order_relaxed));
    for (std::size_t bucket = 0; bucket < kTimingBuckets; ++bucket) {
        snapshot.buckets[bucket] = slot->timingBuckets[bucket].load(std::memory_order_relaxed);
    }
    return snapshot;
}

MetricSnapshot MetricsRegistry::Read(std::uint32_t index) const {
    MetricSnapshot snapshot;
    const Slot *slot = SlotAt(index);
    RUNIC_ASSERT(slot != nullptr);
    if (slot == nullptr) {
        return snapshot;
    }
    snapshot.kind = slot->kind;
    snapshot.name = slot->name;
    snapshot.domain = slot->domain;
    snapshot.help = slot->help;
    switch (slot->kind) {
    case MetricKind::Counter:
        snapshot.counter = slot->counter.load(std::memory_order_relaxed);
        break;
    case MetricKind::Gauge:
        snapshot.gauge = slot->gauge.load(std::memory_order_relaxed);
        break;
    case MetricKind::Timing:
        snapshot.timing = Read(TimingId(index));
        break;
    }
    return snapshot;
}

} // namespace Runic::Core
