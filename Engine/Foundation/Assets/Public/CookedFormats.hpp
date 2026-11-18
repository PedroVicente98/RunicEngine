#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string_view>
#include <vector>

#include "AssetHandle.hpp"
#include "Math.hpp"
#include "PlainIds.hpp"
#include "Result.hpp"
#include "WorldSpace.hpp"

// The native cooked formats (ASSET-1; RunicDocs › World authoring and cooking › Cooked world files
// and packages). The Cooker writes them, client and server read them, and no scene or glTF survives
// into them. Three kinds: an asset's mesh (its visual geometry), an asset's collision (shapes in
// its own space, the same bytes for the server's authority and the client's prediction) and a cell
// (what stands around a world position: placements of assets, and colliders placed on their own).
//
// Every file is a 24-byte header and a payload. The header: the magic "RUNA", the format version
// (1), the kind, the payload's size and its 64-bit FNV-1a checksum; all numbers little-endian,
// floats IEEE-754. A reader checks everything before it trusts a byte — the header, the sizes, the
// checksum, that indices stay inside their arrays, that numbers are finite and rotations are unit
// quaternions — and says what is wrong and where.

namespace Runic::Assets {

inline constexpr std::uint16_t kCookedFormatVersion = 1;
inline constexpr std::size_t kCookedHeaderBytes = 24;
inline constexpr std::string_view kCookedMagic = "RUNA";
// The file names the Cooker gives each kind.
inline constexpr std::string_view kMeshExtension = ".rmesh";
inline constexpr std::string_view kCollisionExtension = ".rcol";
inline constexpr std::string_view kCellExtension = ".rcell";
// How far a rotation's length may stray from 1.
inline constexpr float kUnitQuaternionTolerance = 1.0e-3f;

// An asset's visual geometry in its own space, metres: triangles, three indices each, counter-
// clockwise seen from the outside; a normal per vertex, or none.
struct CookedMesh {
    std::vector<Core::Vec3> positions;
    std::vector<Core::Vec3> normals;
    std::vector<std::uint32_t> indices;
    Core::Vec3 boundsMin{0.0f};
    Core::Vec3 boundsMax{0.0f};
};

enum class CollisionShapeKind : std::uint8_t { Box, Sphere, Capsule, Mesh };

// A shape at a pose: a box by its half extents, a sphere by its radius, a capsule standing on y by
// its radius and the half height of its cylinder, a mesh by its index among the collision's
// meshes.
struct CollisionShape {
    CollisionShapeKind kind = CollisionShapeKind::Box;
    Core::Vec3 halfExtents{0.5f};
    float radius = 0.5f;
    float halfHeight = 0.5f;
    std::uint32_t mesh = 0;
    Core::Vec3 position{0.0f};
    Core::Quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
};

// Static triangles, as Physics' mesh shape takes them.
struct CollisionMesh {
    std::vector<Core::Vec3> positions;
    std::vector<std::uint32_t> indices;
};

// An asset's collision in its own space: what a body of it is made of.
struct CookedCollision {
    std::vector<CollisionMesh> meshes;
    std::vector<CollisionShape> shapes;
};

// An asset placed in a cell, relative to the cell's origin. Its ID is stable from cook to cook (the
// same rock, the same ID on server and client). Its collision is the collision asset cooked for
// this placement's scale (none: no collision), so a runtime never scales a shape.
struct CellPlacement {
    std::uint64_t id = 0;
    Core::AssetId mesh;
    Core::AssetId collision;
    Core::Vec3 position{0.0f};
    Core::Quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    Core::Vec3 scale{1.0f};
};

// A collider placed on its own (an invisible wall, a stair ramp), relative to the cell's origin: a
// box, a sphere or a capsule, never a mesh.
struct CellCollider {
    std::uint64_t id = 0;
    CollisionShape shape;
};

// What stands around a world position: its placements and colliders, small floats near the cell's
// origin, which is a world position in 64 bits.
struct CookedCell {
    Core::WorldPosition origin;
    std::vector<CellPlacement> placements;
    std::vector<CellCollider> colliders;
};

// The bytes of a file of each kind, and back. A reader's error says what is wrong.
std::vector<std::byte> WriteCookedMesh(const CookedMesh &mesh);
std::vector<std::byte> WriteCookedCollision(const CookedCollision &collision);
std::vector<std::byte> WriteCookedCell(const CookedCell &cell);
Core::Result<CookedMesh> ReadCookedMesh(std::span<const std::byte> bytes);
Core::Result<CookedCollision> ReadCookedCollision(std::span<const std::byte> bytes);
Core::Result<CookedCell> ReadCookedCell(std::span<const std::byte> bytes);

// The kind a file says it holds (from its header alone), or why it is not a cooked file.
Core::Result<AssetKind> PeekCookedKind(std::span<const std::byte> bytes);

// Whole files: written through a temporary name and renamed, so a reader never sees half of one.
Core::Result<void> WriteBytesToFile(const std::filesystem::path &path,
                                    std::span<const std::byte> bytes);
Core::Result<std::vector<std::byte>> ReadBytesFromFile(const std::filesystem::path &path);

} // namespace Runic::Assets
