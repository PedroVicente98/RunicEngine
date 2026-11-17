#pragma once

#include <flecs.h>

#include <cstdint>

// How the other parts of this module (the state storage) reach the Flecs world behind an
// EntityWorld. Private to the module: other modules use the public API, or Runic::ECSNative.

namespace Runic::ECS {

class EntityWorld;

namespace Detail {

struct WorldAccess {
    // The Flecs world: operations given to it happen at once.
    static ecs_world_t *World(const EntityWorld &world) noexcept;
    // Where a change that may move an entity goes: the world, or its main stage while changes are
    // held back.
    static ecs_world_t *Target(const EntityWorld &world) noexcept;
    // How many times held-back changes have been applied.
    static std::uint64_t AppliedCount(const EntityWorld &world) noexcept;
};

} // namespace Detail

} // namespace Runic::ECS
