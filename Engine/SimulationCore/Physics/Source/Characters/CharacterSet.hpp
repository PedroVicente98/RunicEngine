#pragma once

#include <Jolt/Jolt.h>

#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Character/CharacterVirtual.h>
#include <Jolt/Physics/Collision/Shape/Shape.h>
#include <Jolt/Physics/PhysicsSystem.h>

#include <cstdint>
#include <unordered_map>
#include <vector>

#include "PhysicsWorld.hpp"
#include "Result.hpp"

// A PhysicsWorld's characters (PHYS-2): Jolt CharacterVirtuals in slots, their handles, their
// capsules and the step that moves them. Private to Runic::Physics.

namespace Runic::Physics::Detail {

// A character handle is its slot's generation above the slot's index; the generation starts at 1,
// so 0 stays "none", and changes each time the slot is reused, so an old handle stays dead.
inline constexpr std::uint32_t kCharacterIndexBits = 20;
inline constexpr std::uint32_t kMaxCharacters = 1u << kCharacterIndexBits; // 1,048,576
inline constexpr std::uint32_t kCharacterIndexMask = kMaxCharacters - 1u;
inline constexpr std::uint32_t kGenerationBits = 32 - kCharacterIndexBits;
inline constexpr std::uint32_t kMaxGeneration = (1u << kGenerationBits) - 1u; // 4,095
inline constexpr std::uint32_t kFirstGeneration = 1;

struct CharacterSlot {
    JPH::Ref<JPH::CharacterVirtual> character; // none: the slot is free
    std::uint32_t generation = kFirstGeneration;
    CollisionLayer layer;
    std::uint64_t owner = 0;
    float stepHeight = 0.0f;
    float stickToFloor = 0.0f;
    CharacterMove move; // for the next step only
};

// The side of a cell of the grid that finds characters near each other: a few person widths, so a
// character's test covers one to four columns.
inline constexpr double kCharacterCellM = 4.0;

// Characters meet each other only when their layers collide. Jolt's own
// CharacterVsCharacterCollisionSimple tests every pair whatever their layers; this one keeps each
// layer's characters in vertical columns of a grid (kCharacterCellM wide, over the 64-bit world
// position), moved as they move, and tests a character only against the characters near it on the
// layers its own meets. Characters on a layer that meets no character layer (the Engine's
// engine:character) cost nothing here. A character's Jolt user data is its slot.
class CharacterCollision final : public JPH::CharacterVsCharacterCollision {
  public:
    CharacterCollision(const std::vector<CharacterSlot> &slots, const CollisionLayers &layers)
        : m_Slots(slots), m_Layers(layers), m_ByLayer(layers.Count() + 1u) {}

    // A character joins at its position; reach is how far it extends sideways from it (its radius
    // and padding).
    void Join(std::uint32_t slot, CollisionLayer layer, JPH::RVec3Arg position, float reach);
    void Leave(std::uint32_t slot, CollisionLayer layer);
    // After a character moved (its update, a teleport): it changes column when it crossed into one.
    void Moved(std::uint32_t slot, CollisionLayer layer, JPH::RVec3Arg position);

    void CollideCharacter(const JPH::CharacterVirtual *character, JPH::RMat44Arg centerOfMass,
                          const JPH::CollideShapeSettings &settings, JPH::RVec3Arg baseOffset,
                          JPH::CollideShapeCollector &collector) const override;
    void CastCharacter(const JPH::CharacterVirtual *character, JPH::RMat44Arg centerOfMass,
                       JPH::Vec3Arg direction, const JPH::ShapeCastSettings &settings,
                       JPH::RVec3Arg baseOffset, JPH::CastShapeCollector &collector) const override;

  private:
    using CellKey = std::uint64_t;
    struct Layer {
        std::vector<std::uint32_t> members; // every slot on the layer, sorted
        std::unordered_map<CellKey, std::vector<std::uint32_t>> cells; // a column's slots
    };

    static CellKey CellOf(JPH::RVec3Arg position) noexcept;
    // Calls visit(other) for every other character whose column overlaps the world-space box
    // [low, high] widened by the characters' reach, on the layers that meet self's, while the
    // collector wants more.
    template <typename Collector, typename Visit>
    void ForEachNear(const JPH::CharacterVirtual *self, JPH::RVec3Arg low, JPH::RVec3Arg high,
                     const Collector &collector, Visit &&visit) const;

    const std::vector<CharacterSlot> &m_Slots;
    const CollisionLayers &m_Layers;
    std::vector<Layer> m_ByLayer;      // per layer handle
    std::vector<CellKey> m_CellOfSlot; // per slot: its column
    float m_Reach = 0.0f;              // the widest character's reach
};

// Characters block each other; they do not push each other. Jolt lets a walking character push a
// standing one along at its own speed (its contact's velocity); here neither moves the other, so
// one character cannot shove another off a ledge.
class CharacterContacts final : public JPH::CharacterContactListener {
  public:
    void OnCharacterContactAdded(const JPH::CharacterVirtual *, const JPH::CharacterContact &,
                                 JPH::CharacterContactSettings &settings) override {
        settings.mCanPushCharacter = false;
    }
    void OnCharacterContactPersisted(const JPH::CharacterVirtual *, const JPH::CharacterContact &,
                                     JPH::CharacterContactSettings &settings) override {
        settings.mCanPushCharacter = false;
    }
};

class CharacterSet {
  public:
    explicit CharacterSet(const CollisionLayers &layers) : m_Collision(m_Slots, layers) {}
    CharacterSet(const CharacterSet &) = delete;
    CharacterSet &operator=(const CharacterSet &) = delete;

    // A new character finds what it touches at once, so one made standing on the ground walks in
    // its first step.
    Core::Result<CharacterHandle> Create(const CharacterDesc &desc, JPH::PhysicsSystem &system,
                                         JPH::TempAllocator &scratch);
    void Remove(CharacterHandle handle);
    CharacterSlot *Find(CharacterHandle handle) noexcept;
    const CharacterSlot *Find(CharacterHandle handle) const noexcept;
    std::uint32_t Count() const noexcept { return m_Count; }
    // Every slot, free ones included (their character is none), for the debug view.
    const std::vector<CharacterSlot> &Slots() const noexcept { return m_Slots; }

    // After a character was put somewhere: it changes column in the grid.
    void Teleported(const CharacterSlot &slot);
    // Finds what a character touches where it stands (after it was made or put somewhere).
    static void Refresh(CharacterSlot &slot, JPH::PhysicsSystem &system,
                        JPH::TempAllocator &scratch);
    // Moves every character by its move, in slot order, then forgets the moves.
    void Update(float dtSeconds, JPH::PhysicsSystem &system, JPH::TempAllocator &scratch);

  private:
    struct Capsule {
        float radius = 0.0f;
        float height = 0.0f;
        JPH::RefConst<JPH::Shape> shape;
    };

    // One shape per capsule size, shared by every character of that size.
    JPH::RefConst<JPH::Shape> CapsuleOf(float radius, float height);

    std::vector<CharacterSlot> m_Slots; // before m_Collision, which reads it
    std::vector<std::uint32_t> m_Free;  // free slots, the last freed reused first
    std::vector<Capsule> m_Capsules;
    CharacterCollision m_Collision;
    CharacterContacts m_Contacts;
    std::uint32_t m_Count = 0;
};

} // namespace Runic::Physics::Detail
