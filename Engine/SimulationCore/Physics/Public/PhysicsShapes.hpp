#pragma once

#include <cstdint>
#include <string_view>
#include <utility>
#include <vector>

#include "Math.hpp"

// Shape descriptions (PHYS-1). A world turns each into an immutable shape in its registry, shared
// by every body that names it ("many skills reuse one SwordArcSmall"). Sizes are metres; a mesh's
// vertices are relative to its body.

namespace Runic::Physics {

enum class ShapeKind : std::uint8_t { Box, Sphere, Capsule, Mesh };

// The fewest metres a box's half extent, a radius or a capsule's half height may be.
inline constexpr float kMinShapeSizeM = 0.001f;
// A new description's sizes: a 1 m box, sphere or capsule radius.
inline constexpr float kDefaultHalfSizeM = 0.5f;

struct ShapeDesc {
    ShapeKind kind = ShapeKind::Box;
    Core::Vec3 halfExtents{kDefaultHalfSizeM}; // Box
    float radius = kDefaultHalfSizeM;          // Sphere, Capsule
    float halfHeight =
        kDefaultHalfSizeM; // Capsule: half the height of its cylinder, the caps not counted
    // Mesh: static triangles, three indices each into vertices, counter-clockwise seen from the
    // outside.
    std::vector<Core::Vec3> vertices;
    std::vector<std::uint32_t> indices;
};

inline ShapeDesc BoxShape(Core::Vec3 halfExtents) {
    ShapeDesc desc;
    desc.kind = ShapeKind::Box;
    desc.halfExtents = halfExtents;
    return desc;
}
inline ShapeDesc SphereShape(float radius) {
    ShapeDesc desc;
    desc.kind = ShapeKind::Sphere;
    desc.radius = radius;
    return desc;
}
// A capsule standing on y: its full height is 2 × (halfHeight + radius).
inline ShapeDesc CapsuleShape(float halfHeight, float radius) {
    ShapeDesc desc;
    desc.kind = ShapeKind::Capsule;
    desc.halfHeight = halfHeight;
    desc.radius = radius;
    return desc;
}
inline ShapeDesc MeshShape(std::vector<Core::Vec3> vertices, std::vector<std::uint32_t> indices) {
    ShapeDesc desc;
    desc.kind = ShapeKind::Mesh;
    desc.vertices = std::move(vertices);
    desc.indices = std::move(indices);
    return desc;
}

} // namespace Runic::Physics
