#pragma once

#include <cstdint>
#include <format>
#include <string>
#include <string_view>

// How the inspectors name what they show, so the world dock and the physics dock name an entity
// the same way. Private to Runic::GodotTools.

namespace Runic::GodotTools::Detail {

inline constexpr std::string_view kEntityLabelFormat = "entity {}";

// The part of an entity handle that names the entity; the rest counts its reuses.
inline constexpr std::uint64_t kEntityIndexMask = 0xFFFF'FFFFull;

inline std::string EntityLabel(std::uint64_t entity) {
    return std::format(kEntityLabelFormat, entity & kEntityIndexMask);
}

} // namespace Runic::GodotTools::Detail
