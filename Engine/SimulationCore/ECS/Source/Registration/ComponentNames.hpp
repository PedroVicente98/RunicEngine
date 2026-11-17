#pragma once

#include <string_view>

// Helpers the registration and the world share (private to the module).

namespace Runic::ECS::Detail {

// True for "namespace:name", each part a lowercase letter followed by lowercase letters, digits
// and underscores: "engine:test_position", "game:mounted".
bool IsNamespacedName(std::string_view name) noexcept;

} // namespace Runic::ECS::Detail
