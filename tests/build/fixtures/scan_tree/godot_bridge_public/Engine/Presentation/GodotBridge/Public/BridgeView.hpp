#pragma once

// Forbidden: GodotBridge keeps godot-cpp private; only GodotTools has the public-header exception.
#include <godot_cpp/variant/vector3.hpp>

namespace Runic::GodotBridge {
godot::Vector3 Origin();
}
