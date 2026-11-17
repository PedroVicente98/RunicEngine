// The Presentation layer (PresentationLayer.hpp): the camera, the extractors and the frame they
// write, one frame per update.

#include "PresentationLayer.hpp"

#include <string>

#include "Assert.hpp"

namespace Runic::Client {

struct PresentationLayer::State {
    explicit State(const PresentationLayerSpec &spec)
        : frames(spec.frames), world(spec.world), extractors(spec.metrics) {}

    RenderData::RenderFrameExchange *frames;
    const ECS::EntityWorld *world;
    ExtractorRegistry extractors;
    ClientCamera camera;
    std::uint64_t written = 0;
};

PresentationLayer::PresentationLayer(const PresentationLayerSpec &spec)
    : Layer(std::string(kPresentationLayerName)) {
    RUNIC_ASSERT_MSG(spec.frames != nullptr, "the Presentation layer needs a frame exchange");
    m_State = std::make_unique<State>(spec);
}

PresentationLayer::~PresentationLayer() = default;

ExtractorRegistry &PresentationLayer::Extractors() noexcept { return m_State->extractors; }

ClientCamera &PresentationLayer::Camera() noexcept { return m_State->camera; }

void PresentationLayer::OnUpdate(const Application::FrameTime &) {
    State &state = *m_State;
    if (state.world != nullptr) {
        state.camera.Update(*state.world);
    }
    RenderData::RenderFrame &frame = state.frames->WriteFrame();
    // The frame is the camera's: its origin is the eye, and every position in it is relative to
    // the eye.
    frame.origin = state.camera.Eye();
    frame.camera = state.camera.ViewFrom(frame.origin);
    frame.debug.Clear(); // the slot still holds what it showed three frames ago

    ExtractionView view;
    view.origin = frame.origin;
    view.camera = state.camera.Eye();
    view.drawDistanceM = state.camera.Lens().farPlaneM;
    view.frame = state.frames->PublishedFrames() + 1;
    state.extractors.Run(view, frame);

    state.frames->Publish();
    ++state.written;
}

std::uint64_t PresentationLayer::FramesWritten() const noexcept { return m_State->written; }

} // namespace Runic::Client
