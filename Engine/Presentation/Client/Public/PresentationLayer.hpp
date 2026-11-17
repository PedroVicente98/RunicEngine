#pragma once

#include <cstdint>
#include <memory>
#include <string_view>

#include "ClientCamera.hpp"
#include "EntityWorld.hpp"
#include "ExtractorRegistry.hpp"
#include "FrameTime.hpp"
#include "Layer.hpp"
#include "MetricsRegistry.hpp"
#include "RenderData/RenderFrameExchange.hpp"

// The Presentation layer (RunicDocs › Client runtime): once per frame, after the layers below it
// have brought the client world up to date, it places the camera, writes one RenderFrame through
// its extractors and publishes it. The GodotLayer above it reads that frame, and only that frame:
// nothing above this layer reads the world.

namespace Runic::Client {

inline constexpr std::string_view kPresentationLayerName = "Presentation";

struct PresentationLayerSpec {
    // Where the frames go: required, and it outlives the layer. The layer is its one writer.
    RenderData::RenderFrameExchange *frames = nullptr;
    // The client world the camera follows entities in; nullptr when there is none. It outlives
    // the layer. Extractors take the world they read when they are made.
    const ECS::EntityWorld *world = nullptr;
    // Where the extractors' timings go; nullptr records nothing. It outlives the layer.
    Core::MetricsRegistry *metrics = nullptr;
};

class PresentationLayer final : public Application::Layer {
  public:
    explicit PresentationLayer(const PresentationLayerSpec &spec);
    ~PresentationLayer() override;

    // Register extractors before the first frame.
    ExtractorRegistry &Extractors() noexcept;
    ClientCamera &Camera() noexcept;

    // Updates the camera, empties the frame's debug geometry, runs the extractors and publishes.
    void OnUpdate(const Application::FrameTime &time) override;

    std::uint64_t FramesWritten() const noexcept;

  private:
    struct State;
    std::unique_ptr<State> m_State;
};

} // namespace Runic::Client
