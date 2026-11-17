#pragma once

#include <string_view>

#include "EntityWorld.hpp"
#include "Math.hpp"
#include "PlainIds.hpp"
#include "RegionFrame.hpp"
#include "WorldSpace.hpp"

// The components that place things in the world. An entity's transform lives in its EntityWorld
// with a 64-bit position (RunicDocs: "Flecs holds the truth"); a region is an entity too.

namespace Runic::World {

// A rotation that turns nothing (w = 1).
inline constexpr Core::Quat kNoRotation{1.0f, 0.0f, 0.0f, 0.0f};

// Where an entity is and how it is turned. The position is a place in the world of the entity's
// simulation; nothing in it is relative to a cell or a region.
struct WorldTransform {
    Core::WorldPosition position;
    Core::Quat rotation = kNoRotation;
};

// An authored content area: the world it belongs to and its own coordinate frame. The entity
// that has it is the region.
struct Region {
    Core::WorldId world;
    RegionFrame frame;
};

// The names the components are registered under.
inline constexpr std::string_view kWorldTransformName = "engine:world_transform";
inline constexpr std::string_view kRegionName = "engine:region";

// Registers WorldTransform and Region in a world, with formatters for tools. Calling it again
// does nothing.
void RegisterWorldComponents(ECS::EntityWorld &world);

} // namespace Runic::World
