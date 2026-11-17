#pragma once

#include <flecs.h>

#include "Entity.hpp"
#include "EntityWorld.hpp"

// Runic::ECSNative: the opt-in door to the native Flecs world of an EntityWorld. Discouraged:
// module code uses the Runic API (EntityWorld, EntityQuery, the state API), which is what keeps
// the conventions and lets the storage change. A module that still needs Flecs itself links
// Runic::ECSNative PRIVATE, and the build lists every module that does (cmake/README.md ›
// runic_add_native_target). What is done through this door is not covered by the world's rules:
// an entity made here is not counted by EntityWorld::EntityCount, and a Flecs system or observer
// made here runs outside the simulation's phases.

namespace Runic::ECS::Native {

// The Flecs world behind an EntityWorld. The EntityWorld still owns it.
inline ecs_world_t *WorldOf(const EntityWorld &world) noexcept {
    return static_cast<ecs_world_t *>(world.NativeWorldHandle());
}

inline ecs_entity_t ToNative(Entity entity) noexcept { return entity.Raw(); }

inline Entity FromNative(ecs_entity_t entity) noexcept { return Entity::FromRaw(entity); }

// The Flecs id of a component type registered in the world.
template <class T> ecs_id_t ComponentOf(const EntityWorld &world) noexcept {
    return world.ComponentIdOf<T>();
}

} // namespace Runic::ECS::Native
