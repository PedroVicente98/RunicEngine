#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

// The debug view (GDB-2): what the GodotLayer draws from each RenderFrame (RunicDocs › Render data,
// extraction and world UI › Debug layers first; Debug drawing (Engine) › Drawing DebugGeometry with
// Godot). Its layers are toggled from the development overlay or through the GodotLayer.
//
// The physics geometry layer (GDB-4) draws the frame's physics geometry (RenderFrame::physics) the
// same way, moved by the same presenter shift as every shape, so collision lines up with what it
// collides for; its large shapes are cached debug meshes, uploaded once from the frame stream's
// DebugMeshLibrary and drawn by instances. "See what the server sees" draws that layer on top of
// everything, so collision shows through the visuals.

namespace Runic::GodotBridge {

// Slice 2's debug layers (decision D4): the depth-tested batch, the overlay batch and the text;
// slice 8's physics geometry and its "see what the server sees" switch (off at the start).
enum class DebugLayer : std::uint8_t {
    DepthShapes,
    OverlayShapes,
    Text,
    PhysicsGeometry,
    ServerView
};
inline constexpr std::size_t kDebugLayerCount = 5;

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
    // The physics geometry layer (GDB-4): its lines and triangles, the cached meshes' instances it
    // and the debug shapes drew, the meshes held and how many uploads the library has cost so far.
    std::uint32_t physicsLineVertices = 0;
    std::uint32_t physicsTriangleVertices = 0;
    std::uint32_t meshInstances = 0;
    std::uint32_t cachedMeshes = 0;
    std::uint64_t meshUploads = 0;
};

// The nodes the view adds under the scene tree's root window.
inline constexpr std::string_view kDebugViewNodeName = "RunicDebugView"; // a Node3D
inline constexpr std::string_view kDebugCameraNodeName = "RunicDebugCamera";
inline constexpr std::string_view kDevelopmentOverlayNodeName = "RunicDevelopmentOverlay";
// The development overlay's CheckBox for a layer.
std::string_view DebugLayerToggleName(DebugLayer layer) noexcept;

} // namespace Runic::GodotBridge
