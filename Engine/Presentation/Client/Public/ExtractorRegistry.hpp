#pragma once

#include <cstdint>
#include <memory>
#include <string_view>
#include <utility>

#include "MetricsRegistry.hpp"
#include "RenderData/Camera.hpp"
#include "RenderData/RenderFrame.hpp"
#include "Time.hpp"
#include "WorldSpace.hpp"

// Render extraction (RunicDocs › Render data, extraction and world UI › Extraction): each frame
// the Presentation layer runs every registered extractor, in the order they were registered, and
// each reads the client world read-only and writes its kinds of render data into the frame. The
// Engine registers its extractors under engine: names, the Game its own under game: names.
// Extractors never change the simulation.

namespace Runic::Client {

// What every extractor knows about the frame being written.
struct ExtractionView {
    // The frame's origin: every position written into the frame is relative to it.
    Core::WorldPosition origin;
    // Where the camera is in the world (the origin, in CLI-1).
    Core::WorldPosition camera;
    // Nothing further from the camera than this is drawn: the camera's far plane.
    float drawDistanceM = RenderData::kDefaultFarPlaneM;
    // The number the frame will carry once published.
    std::uint64_t frame = 0;
};

// One extractor. It keeps what it needs from setup (its queries, its writers) and does nothing
// but read and write the frame when it runs.
class Extractor {
  public:
    virtual ~Extractor() = default;
    // Called once per frame on the thread that runs the Presentation layer.
    virtual void Extract(const ExtractionView &view, RenderData::RenderFrame &frame) = 0;
};

// An extractor as tools see it. The name stays valid while the registry lives.
struct ExtractorInfo {
    std::string_view name;
    Core::Duration lastTime{}; // what its last run took; zero without a metrics registry
};

// The time each extractor took per frame, followed by its name; and the frames extracted.
inline constexpr std::string_view kExtractTimeMetricPrefix = "client.extract_time.";
inline constexpr std::string_view kFramesExtractedMetric = "client.frames_extracted";

class ExtractorRegistry {
  public:
    // metrics: where the registry records its timings; nullptr records nothing. It outlives the
    // registry.
    explicit ExtractorRegistry(Core::MetricsRegistry *metrics = nullptr);
    ~ExtractorRegistry();
    ExtractorRegistry(const ExtractorRegistry &) = delete;
    ExtractorRegistry &operator=(const ExtractorRegistry &) = delete;

    // At setup: adds an extractor that runs after the ones registered before it. The name is
    // "namespace:name" and unique in the registry.
    Extractor &Register(std::string_view name, std::unique_ptr<Extractor> extractor);
    template <class T, class... Args> T &Emplace(std::string_view name, Args &&...args) {
        auto extractor = std::make_unique<T>(std::forward<Args>(args)...);
        T &added = *extractor;
        Register(name, std::move(extractor));
        return added;
    }

    std::uint32_t Count() const noexcept;
    ExtractorInfo Describe(std::uint32_t index) const;
    // The extractor registered under name, or nullptr.
    Extractor *Find(std::string_view name) const noexcept;

    // Runs every extractor once, in registration order.
    void Run(const ExtractionView &view, RenderData::RenderFrame &frame);

  private:
    struct State;
    std::unique_ptr<State> m_State;
};

} // namespace Runic::Client
