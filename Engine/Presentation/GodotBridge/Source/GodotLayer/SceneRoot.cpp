// The scene tree's root window (SceneRoot.hpp).

#include "GodotLayer/SceneRoot.hpp"

#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/window.hpp>

namespace Runic::GodotBridge::Detail {

godot::Window *SceneRoot() {
    godot::MainLoop *loop = godot::Engine::get_singleton()->get_main_loop();
    godot::SceneTree *tree = godot::Object::cast_to<godot::SceneTree>(loop);
    return tree != nullptr ? tree->get_root() : nullptr;
}

} // namespace Runic::GodotBridge::Detail
