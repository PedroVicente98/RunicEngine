#pragma once

#include "Layer.hpp"
#include "RenderData/RenderFrameExchange.hpp"

// RunicClient's debug demo (GDB-2, slice 2's runnable result): a grid, a camera, colored shapes
// with opacity and text, written into each RenderFrame through the debug-draw API and drawn by
// Godot. A neutral scene with no game content; the client's own extraction replaces it (CLI-1,
// slice 5).

namespace Runic::Programs {

class DebugDemoLayer final : public Application::Layer {
  public:
    explicit DebugDemoLayer(RenderData::RenderFrameExchange &frames);
    void OnUpdate(const Application::FrameTime &time) override;

  private:
    RenderData::RenderFrameExchange &m_Frames;
};

} // namespace Runic::Programs
