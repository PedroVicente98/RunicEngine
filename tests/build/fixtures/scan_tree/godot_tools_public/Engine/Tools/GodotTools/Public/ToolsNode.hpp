#pragma once

// Allowed: godot-cpp types in GodotTools' public headers, so a game's editor library can extend them.
#include <godot_cpp/classes/node3d.hpp>

namespace Runic::GodotTools {
class ToolsNode : public godot::Node3D {};
}
