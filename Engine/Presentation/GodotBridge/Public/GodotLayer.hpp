#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "DebugView.hpp"
#include "GodotRuntime.hpp"
#include "Layer.hpp"
#include "Math.hpp"
#include "RenderData/Color.hpp"
#include "RenderData/DebugGeometry.hpp"
#include "RenderData/RenderFrameExchange.hpp"

namespace Runic::GodotBridge {

// A synthetic left click (press, then the release one frame later) that the GodotLayer pushes into
// Godot's root viewport before a Godot frame, so automated checks can prove what Godot's UI
// consumes. Development and tests only.
struct ScriptedClick {
    std::uint64_t frame = 0; // the Godot frame before which the press is pushed
    float x = 0.0f;
    float y = 0.0f;
};

// A synthetic key press and its release, pushed the same way. key uses Godot's key numbering (as
// KeyEvent::key).
struct ScriptedKey {
    std::uint64_t frame = 0;
    std::uint32_t key = 0;
};

struct GodotLayerSpec {
    GodotTarget target = GodotTarget::Editor;
    std::vector<std::string> arguments; // argv[0] first; every argument reaches Godot unchanged
    std::string libraryPath;            // empty: GodotRuntime::DefaultLibraryPath(target)
    bool forwardInput =
        true; // input Godot's UI did not consume becomes AppEvents (off in the RunicEditor)
    std::vector<ScriptedClick> scriptedClicks;
    std::vector<ScriptedKey> scriptedKeys;
    // The frames this layer shows (GDB-2): their camera and debug geometry. The GodotLayer is the
    // exchange's reader, and the exchange must outlive the layer. nullptr: the layer adds nothing
    // to the scene (the RunicEditor).
    RenderData::RenderFrameExchange *frames = nullptr;
    // The development overlay: the debug layers' toggles and what the last frame drew. Development
    // clients only.
    bool developmentOverlay = false;
};

// The LibGodot instance at the top of the client's LayerStack (RunicDocs › Godot integration
// (Engine); Client runtime). OnAttach starts Godot (the window comes from its DisplayServer, unless
// --headless); OnRender, the last step of every frame because this is the top layer, runs one Godot
// frame; the input Godot's UI did not consume is posted as AppEvents; when Godot quits (its window
// closed, --quit-after), the layer asks the host to stop. LibGodot allows one instance per process
// and never a second one, so this layer is never popped or replaced while the application runs.
class GodotLayer final : public Application::Layer {
  public:
    explicit GodotLayer(GodotLayerSpec spec);
    ~GodotLayer() override;

    Core::Result<void> OnAttach(Application::LayerContext &context) override;
    void OnDetach() override;
    // Applies the newest frame of GodotLayerSpec::frames, when there is a new one. The GodotLayer
    // is the top layer, so every layer below has written its part of the frame by then.
    void OnUpdate(const Application::FrameTime &time) override;
    void OnRender() override;

    // The debug layers (GDB-2), visible from the start. A change shows from the next frame; the
    // overlay's checkboxes follow it.
    void SetDebugLayerVisible(DebugLayer layer, bool visible);
    bool IsDebugLayerVisible(DebugLayer layer) const noexcept;
    DebugViewStats DebugStats() const noexcept;
    // What Godot holds for one batch and primitive of the last frame, decoded by Godot, for checks.
    // False when that surface is empty or there is no debug view.
    bool ReadBackDebugSurface(RenderData::DebugBatch batch, RenderData::DebugPrimitive primitive,
                              std::vector<Core::Vec3> &positions,
                              std::vector<RenderData::PackedColor> &colors) const;

    // Before the layer attaches: add registration hooks here (GodotTools registers the
    // RunicEditor's classes).
    GodotRuntime &Runtime() noexcept;
    bool IsHeadless() const noexcept;
    std::uint64_t GodotFrames() const noexcept;

  private:
    struct Impl;
    std::unique_ptr<Impl> m_Impl;
};

} // namespace Runic::GodotBridge
