#pragma once

#include <cstdint>
#include <memory>
#include <string_view>
#include <vector>

#include "CoordinateSpace.hpp"
#include "Math.hpp"
#include "PlainIds.hpp"
#include "Result.hpp"
#include "SpatialIndex.hpp"
#include "WorldComponents.hpp"
#include "WorldSpace.hpp"

// Authored volumes (WORLD-2; RunicDocs › World space coordinates, cells and activation › Volumes
// for caves and rules; Simulation core › Flecs and Jolt together). A volume is a region of space
// with a meaning: a cave under the surface, a quest area, a spawn area, a flight zone, a wall, a
// ship's deck. It is placed in the RunicEditor and cooked with the cells it touches. Its kind,
// whose meaning the Game gives, says what it is, and the kind picks where it lives:
//
//   - a static rule area (a cave, a quest area, a spawn area) lives in World's own index of the
//     cells it touches and is asked "is this point inside?" exactly; it never becomes a Physics
//     body;
//   - a volume that moves with an entity (a ship's deck, a boss aura) is a Physics sensor body;
//   - solid collision (a wall) is a static Physics body in its object layer.
//
// World decides, and lists the volumes that need a body; whoever owns the physics world makes
// them. Nothing in the Engine knows surface, sky or underground: a cave is a volume beneath the
// surface in the same coordinates. An indivisible volume (an island, a castle, a docked ship) is
// one that no ownership boundary may cut: World says which cells it touches.

namespace Runic::World {

struct VolumeIdTag;
struct VolumeKindIdTag;
// A volume of a VolumeSet: a slot and its generation, so a removed volume's ID never names the
// volume that takes its slot.
using VolumeId = Core::Id<VolumeIdTag, std::uint64_t>;
using VolumeKindId = Core::Id<VolumeKindIdTag, std::uint32_t>;

enum class VolumeBackend : std::uint8_t {
    SpatialIndex, // a static rule area: in World's index, asked by point; no Physics body
    SensorBody,   // moves with an entity: a Physics sensor body
    StaticBody,   // solid collision: a static Physics body
};

struct VolumeKindDesc {
    std::string_view name;      // namespaced, such as "engine:test_cave" or "game:quest_area"
    bool solid = false;         // it blocks movement: a static body
    bool followsEntity = false; // it moves with an entity: a sensor body
};

struct VolumeKindInfo {
    std::string_view name;
    VolumeBackend backend = VolumeBackend::SpatialIndex;
};

enum class VolumePartShape : std::uint8_t { Box, Sphere, Cylinder };

// One piece of a volume, in the volume's own space: a box (its half sizes), a sphere (its radius)
// or a cylinder (its radius and its half height along its own Y), moved by offset and turned by
// rotation. A volume is the union of its parts.
struct VolumePart {
    VolumePartShape shape = VolumePartShape::Box;
    Core::Vec3 halfExtents{0.0f};
    float radius = 0.0f;
    float halfHeight = 0.0f;
    Core::Vec3 offset{0.0f};
    Core::Quat rotation = kNoRotation;
};

inline VolumePart BoxPart(Core::Vec3 halfExtents, Core::Vec3 offset = Core::Vec3(0.0f),
                          Core::Quat rotation = kNoRotation) {
    VolumePart part;
    part.shape = VolumePartShape::Box;
    part.halfExtents = halfExtents;
    part.offset = offset;
    part.rotation = rotation;
    return part;
}
inline VolumePart SpherePart(float radius, Core::Vec3 offset = Core::Vec3(0.0f)) {
    VolumePart part;
    part.shape = VolumePartShape::Sphere;
    part.radius = radius;
    part.offset = offset;
    return part;
}
inline VolumePart CylinderPart(float radius, float halfHeight, Core::Vec3 offset = Core::Vec3(0.0f),
                               Core::Quat rotation = kNoRotation) {
    VolumePart part;
    part.shape = VolumePartShape::Cylinder;
    part.radius = radius;
    part.halfHeight = halfHeight;
    part.offset = offset;
    part.rotation = rotation;
    return part;
}

// The smallest part a volume accepts, on every axis: a centimetre, World's smallest unit.
inline constexpr float kMinVolumePartSizeM = 0.01f;

struct VolumeDesc {
    VolumeKindId kind;
    Core::WorldPosition origin;
    Core::Quat rotation = kNoRotation;
    std::vector<VolumePart> parts;
    bool indivisible = false; // no ownership boundary may cut it
    std::uint64_t owner = 0;  // who placed it: a cooked placement, an entity; handed back
};

struct VolumeInfo {
    VolumeKindId kind;
    VolumeBackend backend = VolumeBackend::SpatialIndex;
    bool indivisible = false;
    std::uint64_t owner = 0;
    Core::WorldPosition origin;
    Core::Quat rotation = kNoRotation;
    std::uint32_t parts = 0;
    SpatialBounds bounds; // a box around every part, in the world
};

struct VolumeSetStats {
    std::uint32_t kinds = 0;
    std::uint32_t volumes = 0;
    std::uint32_t indexed = 0; // rule areas in World's index
    std::uint32_t physics = 0; // volumes that need a Physics body
    std::uint32_t indivisible = 0;
    std::uint32_t cells = 0; // cells the index's volumes occupy
};

class VolumeSet {
  public:
    explicit VolumeSet(const CoordinateSpace &space = {});
    ~VolumeSet();

    VolumeSet(const VolumeSet &) = delete;
    VolumeSet &operator=(const VolumeSet &) = delete;

    // ---- Kinds ---------------------------------------------------------------------------------
    // Refused: a name without a namespace, a name taken, a kind both solid and following an
    // entity.
    Core::Result<VolumeKindId> AddKind(const VolumeKindDesc &desc);
    // The kind of that name, or an invalid ID.
    VolumeKindId FindKind(std::string_view name) const noexcept;
    VolumeKindInfo DescribeKind(VolumeKindId kind) const;

    // ---- Volumes -------------------------------------------------------------------------------
    // Refused: a kind this set does not have, no parts, a part smaller than kMinVolumePartSizeM.
    Core::Result<VolumeId> Add(const VolumeDesc &desc);
    void Remove(VolumeId volume);
    bool IsVolume(VolumeId volume) const noexcept;
    // Moves a volume that follows an entity (asserted: static volumes never move).
    void SetPose(VolumeId volume, Core::WorldPosition origin, Core::Quat rotation);
    VolumeInfo Describe(VolumeId volume) const;
    const std::vector<VolumePart> &PartsOf(VolumeId volume) const;
    // The cells a volume touches: those an indivisible volume keeps with one simulation.
    CellRange CellsOf(VolumeId volume) const;
    // Every volume, ascending by ID, in place of what out held.
    void Volumes(std::vector<VolumeId> &out) const;

    // ---- Point queries -------------------------------------------------------------------------
    // Whether a point is inside the volume or on its surface: in at least one of its parts, tested
    // exactly in 64-bit from the volume's origin. Any backend.
    bool IsInside(VolumeId volume, Core::WorldPosition point) const;
    // Appends the rule areas (VolumeBackend::SpatialIndex) a point is inside, ascending by ID:
    // the volumes of the point's cell, each tested exactly.
    void VolumesAt(Core::WorldPosition point, std::vector<VolumeId> &out) const;
    // The volumes that need a Physics body (sensors and solids), ascending by ID, in place of what
    // out held: what the owner of the physics world turns into bodies. A rule area is never among
    // them.
    void PhysicsVolumes(std::vector<VolumeId> &out) const;

    VolumeSetStats Stats() const noexcept;

  private:
    struct State;
    std::unique_ptr<State> m_State;
};

} // namespace Runic::World
