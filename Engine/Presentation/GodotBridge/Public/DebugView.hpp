#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

// The debug view (GDB-2): what the GodotLayer draws from each RenderFrame (RunicDocs › Render data,
// extraction and world UI › Debug layers first; Debug drawing (Engine) › Drawing DebugGeometry with
// Godot). Its layers are toggled from the development overlay or through the GodotLayer.

namespace Runic::GodotBridge {

// Slice 2's debug layers (decision D4): the depth-tested batch, the overlay batch and the text.
enum class DebugLayer : std::uint8_t { DepthShapes, OverlayShapes, Text };
inline constexpr std::size_t kDebugLayerCount = 3;

// What the development overlay calls a layer.
std::string_view DebugLayerLabel(DebugLayer layer) noexcept;

// What the last frame the view applied drew, for the overlay, logs and checks. A hidden layer draws
// nothing.
struct DebugViewStats {
    std::uint64_t frame = 0; // the RenderFrame number shown; 0 before the first
    std::uint32_t lineVertices = 0;
    std::uint32_t triangleVertices = 0;
    std::uint32_t texts = 0;
    // Mesh surfaces built for that frame: one per non-empty batch and primitive.
    std::uint32_t surfaces = 0;
    // What the frame's DebugGeometry could not hold.
    std::uint32_t droppedVertices = 0;
    std::uint32_t droppedTexts = 0;
    // Godot's surface layout is DebugGeometry's, checked when the view starts; when it is not, the
    // view draws no geometry.
    bool layoutMatches = false;
};

// The nodes the view adds under the scene tree's root window.
inline constexpr std::string_view kDebugViewNodeName = "RunicDebugView"; // a Node3D
inline constexpr std::string_view kDebugCameraNodeName = "RunicDebugCamera";
inline constexpr std::string_view kDevelopmentOverlayNodeName = "RunicDevelopmentOverlay";
// The development overlay's CheckBox for a layer.
std::string_view DebugLayerToggleName(DebugLayer layer) noexcept;

} // namespace Runic::GodotBridge
