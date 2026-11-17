// Characters (PhysicsWorld.hpp, PHYS-2) on Jolt's CharacterVirtual: upright capsules with their
// feet at their position, moved by collision checks before the bodies step.

// Jolt.h comes before any other Jolt header.
#include <Jolt/Jolt.h>

#include <Jolt/Geometry/RayAABox.h>
#include <Jolt/Physics/Collision/CollideShape.h>
#include <Jolt/Physics/Collision/CollisionDispatch.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/ShapeCast.h>

#include <algorithm>
#include <cmath>
#include <format>
#include <numbers>

#include "Assert.hpp"
#include "Characters/CharacterSet.hpp"
#include "PhysicsWorld/WorldState.hpp"

namespace Runic::Physics {

namespace Detail {

namespace {

// The world's up: +Y (Core's convention). Characters stand along it and slopes are measured from
// it.
const JPH::Vec3 kUp = JPH::Vec3::sAxisY();
constexpr float kDegreesPerHalfTurn = 180.0f;
constexpr float kHalf = 0.5f;
constexpr float kCapsCount = 2.0f;
constexpr std::uint32_t kCellKeyHalfBits = 32;
// A character on the ground that moves up faster than this, relative to the ground, is leaving it
// (it jumped last step): it keeps its upward speed and cannot jump again. Jolt's samples' value.
constexpr float kLeavingGroundMps = 0.1f;

float Radians(float degrees) noexcept {
    return degrees * std::numbers::pi_v<float> / kDegreesPerHalfTurn;
}

std::uint32_t IndexOf(const JPH::CharacterVirtual *character) noexcept {
    return static_cast<std::uint32_t>(character->GetUserData());
}

CharacterHandle HandleOf(std::uint32_t index, std::uint32_t generation) noexcept {
    return CharacterHandle((generation << kCharacterIndexBits) | index);
}

GroundState GroundOf(JPH::CharacterBase::EGroundState state) noexcept {
    switch (state) {
    case JPH::CharacterBase::EGroundState::OnGround:
        return GroundState::OnGround;
    case JPH::CharacterBase::EGroundState::OnSteepGround:
        return GroundState::OnSteepGround;
    case JPH::CharacterBase::EGroundState::NotSupported:
        return GroundState::NotSupported;
    case JPH::CharacterBase::EGroundState::InAir:
        return GroundState::InAir;
    }
    return GroundState::InAir;
}

// The velocity a character starts its step with (after Jolt's CharacterVirtual sample, without
// inertia): on walkable ground it takes the ground's velocity, and may leave it upwards; elsewhere
// it keeps its vertical speed; gravity always pulls. Horizontally it walks as asked while something
// holds it, and keeps what it had in the air.
JPH::Vec3 StartVelocity(const JPH::CharacterVirtual &character, const CharacterMove &move,
                        JPH::Vec3Arg gravity, float dtSeconds) {
    const JPH::Vec3 current = character.GetLinearVelocity();
    const JPH::Vec3 vertical = current.Dot(kUp) * kUp;
    const JPH::Vec3 ground = character.GetGroundVelocity();
    const bool settled = (vertical - ground).Dot(kUp) < kLeavingGroundMps;
    JPH::Vec3 velocity = vertical;
    if (character.GetGroundState() == JPH::CharacterBase::EGroundState::OnGround && settled) {
        velocity = ground;
        if (move.jumpSpeed > 0.0f) {
            velocity += move.jumpSpeed * kUp;
        }
    }
    velocity += gravity * dtSeconds;
    const JPH::Vec3 wanted = ToJolt(move.velocity);
    velocity += character.IsSupported() ? wanted - wanted.Dot(kUp) * kUp : current - vertical;
    return velocity;
}

} // namespace

CharacterCollision::CellKey CharacterCollision::CellOf(JPH::RVec3Arg position) noexcept {
    // Columns over x and z; the world spans far less than 2^31 cells each way.
    const auto x = static_cast<std::int32_t>(std::floor(position.GetX() / kCharacterCellM));
    const auto z = static_cast<std::int32_t>(std::floor(position.GetZ() / kCharacterCellM));
    return (static_cast<CellKey>(static_cast<std::uint32_t>(x)) << kCellKeyHalfBits) |
           static_cast<std::uint32_t>(z);
}

void CharacterCollision::Join(std::uint32_t slot, CollisionLayer layer, JPH::RVec3Arg position,
                              float reach) {
    Layer &own = m_ByLayer[layer.Value()];
    own.members.insert(std::lower_bound(own.members.begin(), own.members.end(), slot), slot);
    if (m_CellOfSlot.size() <= slot) {
        m_CellOfSlot.resize(slot + 1u);
    }
    const CellKey cell = CellOf(position);
    m_CellOfSlot[slot] = cell;
    own.cells[cell].push_back(slot);
    m_Reach = std::max(m_Reach, reach);
}

namespace {

void Unlist(std::unordered_map<std::uint64_t, std::vector<std::uint32_t>> &cells,
            std::uint64_t cell, std::uint32_t slot) {
    const auto column = cells.find(cell);
    if (column == cells.end()) {
        return;
    }
    std::vector<std::uint32_t> &slots = column->second;
    const auto found = std::find(slots.begin(), slots.end(), slot);
    if (found != slots.end()) {
        *found = slots.back();
        slots.pop_back();
    }
    if (slots.empty()) {
        cells.erase(column); // a column nobody stands in costs nothing
    }
}

} // namespace

void CharacterCollision::Leave(std::uint32_t slot, CollisionLayer layer) {
    Layer &own = m_ByLayer[layer.Value()];
    const auto found = std::lower_bound(own.members.begin(), own.members.end(), slot);
    if (found != own.members.end() && *found == slot) {
        own.members.erase(found);
    }
    Unlist(own.cells, m_CellOfSlot[slot], slot);
}

void CharacterCollision::Moved(std::uint32_t slot, CollisionLayer layer, JPH::RVec3Arg position) {
    const CellKey cell = CellOf(position);
    if (cell == m_CellOfSlot[slot]) {
        return; // the usual case: still in its column
    }
    Layer &own = m_ByLayer[layer.Value()];
    Unlist(own.cells, m_CellOfSlot[slot], slot);
    own.cells[cell].push_back(slot);
    m_CellOfSlot[slot] = cell;
}

template <typename Collector, typename Visit>
void CharacterCollision::ForEachNear(const JPH::CharacterVirtual *self, JPH::RVec3Arg low,
                                     JPH::RVec3Arg high, const Collector &collector,
                                     Visit &&visit) const {
    const CollisionLayer own = m_Slots[IndexOf(self)].layer;
    const auto reach = static_cast<double>(m_Reach);
    const auto x0 = static_cast<std::int64_t>(std::floor((low.GetX() - reach) / kCharacterCellM));
    const auto x1 = static_cast<std::int64_t>(std::floor((high.GetX() + reach) / kCharacterCellM));
    const auto z0 = static_cast<std::int64_t>(std::floor((low.GetZ() - reach) / kCharacterCellM));
    const auto z1 = static_cast<std::int64_t>(std::floor((high.GetZ() + reach) / kCharacterCellM));
    const auto columns = static_cast<std::uint64_t>((x1 - x0 + 1) * (z1 - z0 + 1));
    auto test = [&](std::uint32_t index) {
        const JPH::CharacterVirtual *other = m_Slots[index].character.GetPtr();
        if (other != self) {
            visit(*other);
        }
    };
    for (std::size_t layer = 1; layer < m_ByLayer.size(); ++layer) {
        const Layer &theirs = m_ByLayer[layer];
        if (theirs.members.empty() ||
            !m_Layers.Collides(own, CollisionLayer(static_cast<std::uint16_t>(layer)))) {
            continue;
        }
        // A long sweep covers more columns than the layer has characters: test them all.
        if (columns > theirs.members.size()) {
            for (const std::uint32_t index : theirs.members) {
                if (collector.ShouldEarlyOut()) {
                    return;
                }
                test(index);
            }
            continue;
        }
        for (std::int64_t x = x0; x <= x1; ++x) {
            for (std::int64_t z = z0; z <= z1; ++z) {
                const CellKey cell =
                    (static_cast<CellKey>(static_cast<std::uint32_t>(x)) << kCellKeyHalfBits) |
                    static_cast<std::uint32_t>(z);
                const auto column = theirs.cells.find(cell);
                if (column == theirs.cells.end()) {
                    continue;
                }
                for (const std::uint32_t index : column->second) {
                    if (collector.ShouldEarlyOut()) {
                        return;
                    }
                    test(index);
                }
            }
        }
    }
}

void CharacterCollision::CollideCharacter(const JPH::CharacterVirtual *character,
                                          JPH::RMat44Arg centerOfMass,
                                          const JPH::CollideShapeSettings &settings,
                                          JPH::RVec3Arg baseOffset,
                                          JPH::CollideShapeCollector &collector) const {
    // Everything relative to baseOffset (near the character), where floats are precise.
    const JPH::Mat44 transform1 = centerOfMass.PostTranslated(-baseOffset).ToMat44();
    const JPH::Shape *shape1 = character->GetShape();
    const JPH::AABox bounds1 = shape1->GetWorldSpaceBounds(transform1, JPH::Vec3::sOne());
    const JPH::Vec3 margin = JPH::Vec3::sReplicate(settings.mMaxSeparationDistance);
    JPH::CollideShapeSettings padded = settings;
    ForEachNear(
        character, baseOffset + (bounds1.mMin - margin), baseOffset + (bounds1.mMax + margin),
        collector, [&](const JPH::CharacterVirtual &other) {
            const JPH::Mat44 transform2 =
                other.GetCenterOfMassTransform().PostTranslated(-baseOffset).ToMat44();
            // The other character's padding is part of its outer shell.
            padded.mMaxSeparationDistance =
                settings.mMaxSeparationDistance + other.GetCharacterPadding();
            JPH::AABox bounds2 =
                other.GetShape()->GetWorldSpaceBounds(transform2, JPH::Vec3::sOne());
            bounds2.ExpandBy(JPH::Vec3::sReplicate(padded.mMaxSeparationDistance));
            if (!bounds1.Overlaps(bounds2)) {
                return;
            }
            // Jolt's collector learns which character it hit through its user data.
            collector.SetUserData(reinterpret_cast<JPH::uint64>(&other));
            JPH::CollisionDispatch::sCollideShapeVsShape(
                shape1, other.GetShape(), JPH::Vec3::sOne(), JPH::Vec3::sOne(), transform1,
                transform2, JPH::SubShapeIDCreator(), JPH::SubShapeIDCreator(), padded, collector);
        });
    collector.SetUserData(0);
}

void CharacterCollision::CastCharacter(const JPH::CharacterVirtual *character,
                                       JPH::RMat44Arg centerOfMass, JPH::Vec3Arg direction,
                                       const JPH::ShapeCastSettings &settings,
                                       JPH::RVec3Arg baseOffset,
                                       JPH::CastShapeCollector &collector) const {
    const JPH::Mat44 transform1 = centerOfMass.PostTranslated(-baseOffset).ToMat44();
    const JPH::ShapeCast cast(character->GetShape(), JPH::Vec3::sOne(), transform1, direction);
    const JPH::Vec3 origin = cast.mShapeWorldBounds.GetCenter();
    const JPH::Vec3 extents =
        cast.mShapeWorldBounds.GetExtent() + JPH::Vec3::sReplicate(settings.mExtraConvexRadius);
    // The swept box: where the cast starts and where it ends.
    JPH::AABox swept = cast.mShapeWorldBounds;
    swept.Encapsulate(cast.mShapeWorldBounds.mMin + direction);
    swept.Encapsulate(cast.mShapeWorldBounds.mMax + direction);
    swept.ExpandBy(JPH::Vec3::sReplicate(settings.mExtraConvexRadius));
    JPH::ShapeCastSettings padded = settings;
    ForEachNear(character, baseOffset + swept.mMin, baseOffset + swept.mMax, collector,
                [&](const JPH::CharacterVirtual &other) {
                    const JPH::Mat44 transform2 =
                        other.GetCenterOfMassTransform().PostTranslated(-baseOffset).ToMat44();
                    padded.mExtraConvexRadius =
                        settings.mExtraConvexRadius + other.GetCharacterPadding();
                    JPH::AABox bounds2 =
                        other.GetShape()->GetWorldSpaceBounds(transform2, JPH::Vec3::sOne());
                    bounds2.ExpandBy(extents + JPH::Vec3::sReplicate(other.GetCharacterPadding()));
                    if (!JPH::RayAABoxHits(origin, direction, bounds2.mMin, bounds2.mMax)) {
                        return;
                    }
                    collector.SetUserData(reinterpret_cast<JPH::uint64>(&other));
                    JPH::CollisionDispatch::sCastShapeVsShapeWorldSpace(
                        cast, padded, other.GetShape(), JPH::Vec3::sOne(), {}, transform2,
                        JPH::SubShapeIDCreator(), JPH::SubShapeIDCreator(), collector);
                });
    collector.SetUserData(0);
}

JPH::RefConst<JPH::Shape> CharacterSet::CapsuleOf(float radius, float height) {
    for (const Capsule &capsule : m_Capsules) {
        if (capsule.radius == radius && capsule.height == height) {
            return capsule.shape;
        }
    }
    const float halfCylinder = kHalf * (height - kCapsCount * radius);
    JPH::RefConst<JPH::Shape> shape = new JPH::CapsuleShape(halfCylinder, radius);
    m_Capsules.push_back({radius, height, shape});
    return shape;
}

Core::Result<CharacterHandle> CharacterSet::Create(const CharacterDesc &desc,
                                                   JPH::PhysicsSystem &system,
                                                   JPH::TempAllocator &scratch) {
    if (desc.radius < kMinShapeSizeM ||
        desc.height - kCapsCount * desc.radius < kCapsCount * kMinShapeSizeM) {
        return Core::Fail(std::format(
            "a character needs a radius of at least 1 mm and room between its caps (radius {} m, "
            "height {} m)",
            desc.radius, desc.height));
    }
    if (!(desc.maxSlopeDeg > 0.0f && desc.maxSlopeDeg < kMaxSlopeLimitDeg)) {
        return Core::Fail(std::format("a character's steepest walkable slope is above 0 and below "
                                      "{} degrees, not {}",
                                      kMaxSlopeLimitDeg, desc.maxSlopeDeg));
    }
    if (desc.stepHeight < 0.0f || desc.stickToFloor < 0.0f) {
        return Core::Fail("a character's step height and floor distance are 0 or more");
    }
    std::uint32_t index = 0;
    if (!m_Free.empty()) {
        index = m_Free.back();
        m_Free.pop_back();
    } else if (m_Slots.size() < kMaxCharacters) {
        index = static_cast<std::uint32_t>(m_Slots.size());
        m_Slots.emplace_back();
    } else {
        return Core::Fail(
            std::format("the PhysicsWorld holds the most characters it can ({})", kMaxCharacters));
    }

    JPH::Ref<JPH::CharacterVirtualSettings> settings = new JPH::CharacterVirtualSettings();
    settings->mShape = CapsuleOf(desc.radius, desc.height);
    settings->mUp = kUp;
    settings->mMaxSlopeAngle = Radians(desc.maxSlopeDeg);
    // The capsule stands on its position: the feet are the bottom of the lower cap.
    settings->mShapeOffset = kHalf * desc.height * kUp;
    // Only contacts on the lower cap can hold it up; the rest of it only collides.
    settings->mSupportingVolume = JPH::Plane(kUp, -desc.radius);

    CharacterSlot &slot = m_Slots[index];
    slot.character = new JPH::CharacterVirtual(settings, ToJolt(desc.pose.position),
                                               ToJolt(desc.pose.rotation), index, &system);
    slot.character->SetCharacterVsCharacterCollision(&m_Collision);
    slot.character->SetListener(&m_Contacts);
    slot.layer = desc.layer;
    slot.owner = desc.owner;
    slot.stepHeight = desc.stepHeight;
    slot.stickToFloor = desc.stickToFloor;
    slot.move = {};
    m_Collision.Join(index, desc.layer, slot.character->GetPosition(),
                     desc.radius + slot.character->GetCharacterPadding());
    Refresh(slot, system, scratch);
    ++m_Count;
    return HandleOf(index, slot.generation);
}

void CharacterSet::Teleported(const CharacterSlot &slot) {
    m_Collision.Moved(IndexOf(slot.character.GetPtr()), slot.layer, slot.character->GetPosition());
}

void CharacterSet::Refresh(CharacterSlot &slot, JPH::PhysicsSystem &system,
                           JPH::TempAllocator &scratch) {
    const JPH::ObjectLayer layer = ToJolt(slot.layer);
    slot.character->RefreshContacts(system.GetDefaultBroadPhaseLayerFilter(layer),
                                    system.GetDefaultLayerFilter(layer), {}, {}, scratch);
}

CharacterSlot *CharacterSet::Find(CharacterHandle handle) noexcept {
    const auto *self = this;
    return const_cast<CharacterSlot *>(self->Find(handle));
}

const CharacterSlot *CharacterSet::Find(CharacterHandle handle) const noexcept {
    if (!handle.IsValid()) {
        return nullptr;
    }
    const std::uint32_t index = handle.Value() & kCharacterIndexMask;
    const std::uint32_t generation = handle.Value() >> kCharacterIndexBits;
    if (index >= m_Slots.size()) {
        return nullptr;
    }
    const CharacterSlot &slot = m_Slots[index];
    return slot.character != nullptr && slot.generation == generation ? &slot : nullptr;
}

void CharacterSet::Remove(CharacterHandle handle) {
    CharacterSlot *slot = Find(handle);
    if (slot == nullptr) {
        return;
    }
    m_Collision.Leave(handle.Value() & kCharacterIndexMask, slot->layer);
    slot->character = nullptr;
    slot->generation = slot->generation == kMaxGeneration ? kFirstGeneration : slot->generation + 1;
    m_Free.push_back(handle.Value() & kCharacterIndexMask);
    --m_Count;
}

void CharacterSet::Update(float dtSeconds, JPH::PhysicsSystem &system,
                          JPH::TempAllocator &scratch) {
    const JPH::Vec3 gravity = system.GetGravity();
    JPH::CharacterVirtual::ExtendedUpdateSettings update;
    for (CharacterSlot &slot : m_Slots) {
        if (slot.character == nullptr) {
            continue;
        }
        JPH::CharacterVirtual &character = *slot.character;
        // The ground may have changed speed since the last step (a platform's new target).
        character.UpdateGroundVelocity();
        character.SetLinearVelocity(StartVelocity(character, slot.move, gravity, dtSeconds));
        update.mStickToFloorStepDown = -slot.stickToFloor * kUp;
        update.mWalkStairsStepUp = slot.stepHeight * kUp;
        const JPH::ObjectLayer layer = ToJolt(slot.layer);
        character.ExtendedUpdate(dtSeconds, gravity, update,
                                 system.GetDefaultBroadPhaseLayerFilter(layer),
                                 system.GetDefaultLayerFilter(layer), {}, {}, scratch);
        m_Collision.Moved(IndexOf(&character), slot.layer, character.GetPosition());
        slot.move = {};
    }
}

} // namespace Detail

Core::Result<CharacterHandle> PhysicsWorld::CreateCharacter(const CharacterDesc &desc) {
    RUNIC_ASSERT_MSG(desc.layer.IsValid() && desc.layer.Value() <= m_State->layers.Table().Count(),
                     "a character names a collision layer the world does not have");
    return m_State->characters.Create(desc, m_State->system, m_State->scratch);
}

void PhysicsWorld::RemoveCharacter(CharacterHandle character) {
    m_State->characters.Remove(character);
}

bool PhysicsWorld::IsCharacter(CharacterHandle character) const noexcept {
    return m_State->characters.Find(character) != nullptr;
}

void PhysicsWorld::MoveCharacter(CharacterHandle character, const CharacterMove &move) {
    Detail::CharacterSlot *slot = m_State->characters.Find(character);
    RUNIC_ASSERT_MSG(slot != nullptr, "the handle names no character");
    slot->move = move;
}

void PhysicsWorld::SetCharacterPose(CharacterHandle character, const Pose &pose) {
    Detail::CharacterSlot *slot = m_State->characters.Find(character);
    RUNIC_ASSERT_MSG(slot != nullptr, "the handle names no character");
    slot->character->SetPosition(Detail::ToJolt(pose.position));
    slot->character->SetRotation(Detail::ToJolt(pose.rotation));
    m_State->characters.Teleported(*slot);
    Detail::CharacterSet::Refresh(*slot, m_State->system, m_State->scratch);
}

CharacterState PhysicsWorld::Character(CharacterHandle character) const {
    const Detail::CharacterSlot *slot = m_State->characters.Find(character);
    RUNIC_ASSERT_MSG(slot != nullptr, "the handle names no character");
    const JPH::CharacterVirtual &jolt = *slot->character;
    CharacterState state;
    state.pose = {Detail::FromJolt(jolt.GetPosition()), Detail::FromJolt(jolt.GetRotation())};
    state.velocity = Detail::FromJolt(jolt.GetLinearVelocity());
    state.ground = Detail::GroundOf(jolt.GetGroundState());
    state.groundNormal = Detail::FromJolt(jolt.GetGroundNormal());
    state.groundVelocity = Detail::FromJolt(jolt.GetGroundVelocity());
    if (!jolt.GetGroundBodyID().IsInvalid()) {
        state.groundBody = Detail::ToHandle(jolt.GetGroundBodyID());
        state.groundOwner = jolt.GetGroundUserData();
    }
    return state;
}

} // namespace Runic::Physics
