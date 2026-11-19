// The development overlay (DevelopmentOverlay.hpp).

#include "Debug/DevelopmentOverlay.hpp"

#include "Debug/DebugLayers.hpp"

#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/panel_container.hpp>
#include <godot_cpp/classes/v_box_container.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>

#include <cstddef>
#include <format>
#include <string>
#include <string_view>

namespace Runic::GodotBridge::Detail {

namespace {

// Above the game's own UI.
constexpr int kOverlayCanvasLayer = 100;
constexpr float kOverlayMarginPx = 12.0f;
constexpr std::string_view kOverlayTitle = "Runic development overlay";
// What the last frame drew, then Godot's frame rate.
constexpr std::string_view kStatsFormat =
    "frame {} · {} line and {} triangle vertices · {} texts · {} dropped · {:.0f} fps";
constexpr const char *kToggledSignal = "toggled";
constexpr std::array<DebugLayer, kDebugLayerCount> kLayers{
    DebugLayer::DepthShapes, DebugLayer::OverlayShapes, DebugLayer::Text};

godot::String ToGodot(std::string_view text) {
    return godot::String::utf8(text.data(), static_cast<int64_t>(text.size()));
}

} // namespace

void RunicDevelopmentOverlay::Build(DebugLayers &layers) {
    m_Layers = &layers;
    set_layer(kOverlayCanvasLayer);

    auto *panel = memnew(godot::PanelContainer);
    panel->set_anchors_preset(godot::Control::PRESET_TOP_RIGHT);
    panel->set_offset(godot::SIDE_LEFT, -kOverlayMarginPx);
    panel->set_offset(godot::SIDE_RIGHT, -kOverlayMarginPx);
    panel->set_offset(godot::SIDE_TOP, kOverlayMarginPx);
    panel->set_offset(godot::SIDE_BOTTOM, kOverlayMarginPx);
    panel->set_h_grow_direction(godot::Control::GROW_DIRECTION_BEGIN);
    add_child(panel);

    auto *column = memnew(godot::VBoxContainer);
    panel->add_child(column);
    auto *title = memnew(godot::Label);
    title->set_text(ToGodot(kOverlayTitle));
    column->add_child(title);
    for (std::size_t i = 0; i < kLayers.size(); ++i) {
        const DebugLayer layer = kLayers[i];
        auto *toggle = memnew(godot::CheckBox);
        toggle->set_name(ToGodot(DebugLayerToggleName(layer)));
        toggle->set_text(ToGodot(DebugLayerLabel(layer)));
        toggle->set_pressed_no_signal(layers.IsLayerVisible(layer));
        toggle->connect(
            kToggledSignal,
            callable_mp(this, &RunicDevelopmentOverlay::OnToggled).bind(static_cast<int>(layer)));
        column->add_child(toggle);
        m_Toggles[i] = toggle;
    }
    m_Stats = memnew(godot::Label);
    column->add_child(m_Stats);
}

void RunicDevelopmentOverlay::Detach() { m_Layers = nullptr; }

void RunicDevelopmentOverlay::FollowLayer(DebugLayer layer, bool visible) {
    if (godot::CheckBox *toggle = m_Toggles[static_cast<std::size_t>(layer)]) {
        toggle->set_pressed_no_signal(visible);
    }
}

void RunicDevelopmentOverlay::ShowStats(const DebugViewStats &stats) {
    if (m_Stats == nullptr) {
        return;
    }
    const std::string line =
        std::format(kStatsFormat, stats.frame, stats.lineVertices, stats.triangleVertices,
                    stats.texts, stats.droppedVertices + stats.droppedTexts,
                    godot::Engine::get_singleton()->get_frames_per_second());
    m_Stats->set_text(ToGodot(line));
}

void RunicDevelopmentOverlay::OnToggled(bool pressed, int layer) {
    if (m_Layers != nullptr) {
        m_Layers->SetLayerVisible(static_cast<DebugLayer>(layer), pressed);
    }
}

void RegisterDebugClasses() { godot::ClassDB::register_internal_class<RunicDevelopmentOverlay>(); }

} // namespace Runic::GodotBridge::Detail
