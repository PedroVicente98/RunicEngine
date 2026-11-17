#pragma once

#include <cstdint>

#include "Math.hpp"
#include "RenderData/Color.hpp"
#include "RenderData/ShapeInstances.hpp"
#include "Time.hpp"
#include "WorldSpace.hpp"

// The presenter (GDB-3; RunicDocs › Render data, extraction and world UI › The presenter; Client
// runtime › Presenter origin): it turns the RenderFrame's instances into RenderingServer instances
// on Godot's main thread, with no node per instance. An instance is created once, by its
// RenderInstanceId, moved only when it changes and freed when it leaves the frame, so a frame in
// which nothing changed costs no RenderingServer call at all.
//
// Godot stays in single precision, so the presenter keeps its own 64-bit origin and places
// everything relative to it. A frame is relative to the camera, whose origin moves every frame; the
// presenter's moves only when the camera has gone more than kRebaseDistanceM away, and then every
// instance is shifted once. Positions inside Godot therefore stay within a few kilometres, where a
// float's step is under half a millimetre.

namespace Runic::GodotBridge {

// How far the frame's origin may drift from the presenter's before the presenter moves its own.
inline constexpr double kRebaseDistanceM = 4'000.0;

// What the presenter did with the last frame it applied, and in total.
struct PresenterStats {
    std::uint64_t frame = 0;       // the RenderFrame number applied; 0 before the first
    std::uint32_t instances = 0;   // instances in Godot after it
    std::uint32_t created = 0;     // in the last frame
    std::uint32_t updated = 0;     // in the last frame: moved, turned, resized or recolored
    std::uint32_t freed = 0;       // in the last frame
    std::uint32_t serverCalls = 0; // RenderingServer calls for instances in the last frame
    bool comparedById = false;     // the last frame was matched by ID, not by its changed list
    Core::Duration applyTime{};    // what applying the last frame took
    std::uint64_t rebases = 0;     // since the presenter started
    Core::WorldPosition origin;    // the presenter's origin: Godot's (0, 0, 0)
};

// An instance as the presenter gave it to Godot, for checks: Godot's coordinates.
struct PresentedInstance {
    RenderData::ShapeKind shape = RenderData::ShapeKind::Box;
    RenderData::PackedColor color = RenderData::kDebugInactive;
    Core::Vec3 position{0.0f}; // relative to the presenter's origin
    Core::Quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    Core::Vec3 scale{1.0f}; // what the unit mesh is scaled by
};

} // namespace Runic::GodotBridge
