#pragma once

#include <cstdint>

#include "PlainIds.hpp"

// The opaque handles Physics gives out (RunicDocs › Simulation core › Flecs and Jolt together: "its
// body or CharacterVirtual is an opaque Physics handle in a component"). A handle is a plain number
// with no meaning outside the PhysicsWorld that made it; it never names a Jolt type.

namespace Runic::Physics {

struct BodyHandleTag;
struct CharacterHandleTag;
struct ShapeIdTag;
struct CollisionLayerTag;

// A body: static, kinematic or dynamic. Stable while the body lives.
using BodyHandle = Core::Id<BodyHandleTag, std::uint32_t>;
// A character (a CharacterVirtual, not a body).
using CharacterHandle = Core::Id<CharacterHandleTag, std::uint32_t>;
// A shape in the world's shape registry: immutable and shared by every body that uses it.
using ShapeId = Core::Id<ShapeIdTag, std::uint32_t>;
// A collision layer (Jolt's object layer), from the world's CollisionLayers.
using CollisionLayer = Core::Id<CollisionLayerTag, std::uint16_t>;

enum class MotionType : std::uint8_t {
    Static,    // terrain, buildings: never moves
    Kinematic, // doors, elevators, platforms: moved by targets, pushes what it meets
    Dynamic,   // ships, crates, physical objects: moved by forces and contacts
};

} // namespace Runic::Physics
