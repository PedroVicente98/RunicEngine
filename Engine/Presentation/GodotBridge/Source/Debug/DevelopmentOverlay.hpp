#pragma once

#include <array>
#include <cstdint>

#include <godot_cpp/classes/canvas_layer.hpp>
#include <godot_cpp/classes/check_box.hpp>
#include <godot_cpp/classes/label.hpp>

#include "DebugView.hpp"

// The development overlay (GDB-2; RunicDocs › Client runtime › Development builds): a panel at the
// window's top-right with one CheckBox per debug layer and a line of what the last frame drew.
// Godot's UI takes the clicks on it, so they never reach the LayerStack. Private to
// Runic::GodotBridge.

namespace Runic::GodotBridge::Detail {

class DebugLayers;

class RunicDevelopmentOverlay : public godot::CanvasLayer {
    GDCLASS(RunicDevelopmentOverlay, godot::CanvasLayer)

  public:
    // Builds the panel; its checkboxes start as the layers are.
    void Build(DebugLayers &layers);
    // Before the debug layers go: the checkboxes stop calling them.
    void Detach();
    // Shows a layer's visibility without calling back (a change made through the GodotLayer).
    void FollowLayer(DebugLayer layer, bool visible);
    void ShowStats(const DebugViewStats &stats);

  protected:
    static void _bind_methods() {}

  private:
    void OnToggled(bool pressed, int layer);

    DebugLayers *m_Layers = nullptr;
    std::array<godot::CheckBox *, kDebugLayerCount> m_Toggles{};
    godot::Label *m_Stats = nullptr;
};

// Registers the overlay's class with Godot; called once at the Scene initialization level.
void RegisterDebugClasses();

} // namespace Runic::GodotBridge::Detail
