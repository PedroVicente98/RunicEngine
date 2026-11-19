#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "Layer.hpp"

namespace Runic::Application {

// The layers of one application: layers bottom to top, then overlays, which always sit above every
// layer. Pushes, pops and replacements are queued and applied only by ApplyPendingChanges, the
// frame's one safe point, so no layer changes while the stack is being walked.
class LayerStack {
  public:
    LayerStack();
    ~LayerStack(); // destroys what it holds; DetachAll first
    LayerStack(const LayerStack &) = delete;
    LayerStack &operator=(const LayerStack &) = delete;

    void PushLayer(std::unique_ptr<Layer> layer);
    void PushOverlay(std::unique_ptr<Layer> overlay);
    void PopLayer(const Layer &layer);
    void ReplaceLayer(const Layer &current, std::unique_ptr<Layer> replacement);
    bool HasPendingChanges() const noexcept;

    // The safe point: applies the queued changes in the order they were asked for; changes asked
    // for meanwhile wait for the next safe point. A layer that fails to attach is destroyed; the
    // first such error is returned after the remaining changes are applied.
    Core::Result<void> ApplyPendingChanges(LayerContext &context);

    void DispatchEvent(AppEvent &event); // overlays top-down, then layers top-down, until handled
    void UpdateBottomUp(const FrameTime &time); // layers bottom-up, then overlays bottom-up
    void RenderBottomUp();                      // the same order
    void DetachAll(); // overlays then layers, top-down, each destroyed after its OnDetach

    std::size_t LayerCount() const noexcept;
    std::size_t OverlayCount() const noexcept;
    std::vector<std::string>
    Names() const; // layers bottom to top, then overlays (for logs and tests)

  private:
    struct Change;

    std::vector<std::unique_ptr<Layer>> m_Layers;
    std::vector<std::unique_ptr<Layer>> m_Overlays;
    std::vector<Change> m_Pending;
};

} // namespace Runic::Application
