#pragma once

namespace godot {
class Window;
} // namespace godot

namespace Runic::GodotBridge::Detail {

// The scene tree's root window: where the input sink, the debug view and the overlay add their
// nodes. nullptr when Godot has no scene tree.
godot::Window *SceneRoot();

} // namespace Runic::GodotBridge::Detail
