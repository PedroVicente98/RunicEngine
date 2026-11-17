// The extractor registry (ExtractorRegistry.hpp): extractors in registration order, each with its
// name and, when there is a metrics registry, its timing.

#include "ExtractorRegistry.hpp"

#include <string>
#include <vector>

#include "Assert.hpp"

namespace Runic::Client {

namespace {

constexpr std::string_view kExtractTimeHelp = "Time one extractor took to write a frame";
constexpr std::string_view kFramesExtractedHelp = "Frames the extractors wrote";
constexpr char kNamespaceSeparator = ':';

} // namespace

struct ExtractorRegistry::State {
    struct Entry {
        std::string name;
        std::unique_ptr<Extractor> extractor;
        Core::TimingId time;
        Core::Duration lastTime{};
    };

    Core::MetricsRegistry *metrics = nullptr;
    std::vector<Entry> entries;
    Core::CounterId frames;
};

ExtractorRegistry::ExtractorRegistry(Core::MetricsRegistry *metrics)
    : m_State(std::make_unique<State>()) {
    m_State->metrics = metrics;
    if (metrics != nullptr) {
        m_State->frames = metrics->AddCounter(
            {kFramesExtractedMetric, Core::kNoMetricDomain, kFramesExtractedHelp});
    }
}

ExtractorRegistry::~ExtractorRegistry() = default;

Extractor &ExtractorRegistry::Register(std::string_view name,
                                       std::unique_ptr<Extractor> extractor) {
    RUNIC_ASSERT_MSG(extractor != nullptr, "an extractor was registered as nothing");
    RUNIC_ASSERT_MSG(name.find(kNamespaceSeparator) != std::string_view::npos,
                     "an extractor's name is namespace:name");
    RUNIC_ASSERT_MSG(Find(name) == nullptr, "two extractors were registered under one name");
    State::Entry entry;
    entry.name = name;
    entry.extractor = std::move(extractor);
    if (m_State->metrics != nullptr) {
        entry.time =
            m_State->metrics->AddTiming({std::string(kExtractTimeMetricPrefix) + entry.name,
                                         Core::kNoMetricDomain, kExtractTimeHelp});
    }
    m_State->entries.push_back(std::move(entry));
    return *m_State->entries.back().extractor;
}

std::uint32_t ExtractorRegistry::Count() const noexcept {
    return static_cast<std::uint32_t>(m_State->entries.size());
}

ExtractorInfo ExtractorRegistry::Describe(std::uint32_t index) const {
    RUNIC_ASSERT(index < m_State->entries.size());
    const State::Entry &entry = m_State->entries[index];
    return {entry.name, entry.lastTime};
}

Extractor *ExtractorRegistry::Find(std::string_view name) const noexcept {
    for (const State::Entry &entry : m_State->entries) {
        if (entry.name == name) {
            return entry.extractor.get();
        }
    }
    return nullptr;
}

void ExtractorRegistry::Run(const ExtractionView &view, RenderData::RenderFrame &frame) {
    Core::MetricsRegistry *metrics = m_State->metrics;
    if (metrics == nullptr) {
        for (State::Entry &entry : m_State->entries) {
            entry.extractor->Extract(view, frame);
        }
        return;
    }
    Core::TimePoint before = Core::MonotonicNow();
    for (State::Entry &entry : m_State->entries) {
        entry.extractor->Extract(view, frame);
        const Core::TimePoint after = Core::MonotonicNow();
        entry.lastTime = after - before;
        metrics->Record(entry.time, entry.lastTime);
        before = after;
    }
    metrics->Increment(m_State->frames);
}

} // namespace Runic::Client
