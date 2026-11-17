#pragma once

#include <Jolt/Jolt.h>

#include <Jolt/Core/JobSystemSingleThreaded.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Collision/ContactListener.h>
#include <Jolt/Physics/Collision/Shape/Shape.h>
#include <Jolt/Physics/PhysicsSystem.h>

#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "CollisionLayers/JoltLayers.hpp"
#include "PhysicsWorld.hpp"
#include "PhysicsWorld/JoltRuntime.hpp"

// The state behind a PhysicsWorld, shared by the files that implement it (PhysicsWorld/,
// Characters/). Private to Runic::Physics.

namespace Runic::Physics::Detail {

inline JPH::Vec3 ToJolt(Core::Vec3 v) noexcept { return {v.x, v.y, v.z}; }
inline Core::Vec3 FromJolt(JPH::Vec3Arg v) noexcept { return {v.GetX(), v.GetY(), v.GetZ()}; }
inline JPH::RVec3 ToJolt(Core::WorldPosition p) noexcept { return {p.x, p.y, p.z}; }
inline Core::WorldPosition FromJolt(JPH::RVec3Arg p) noexcept {
    return {p.GetX(), p.GetY(), p.GetZ()};
}
// Jolt's quaternion is (x, y, z, w); GLM's constructor takes w first.
inline JPH::Quat ToJolt(Core::Quat q) noexcept { return {q.x, q.y, q.z, q.w}; }
inline Core::Quat FromJolt(JPH::QuatArg q) noexcept {
    return {q.GetW(), q.GetX(), q.GetY(), q.GetZ()};
}

// A body handle is Jolt's index-and-sequence number plus one, so 0 stays "none".
inline BodyHandle ToHandle(JPH::BodyID id) noexcept {
    return BodyHandle(id.GetIndexAndSequenceNumber() + 1u);
}
inline JPH::BodyID ToBodyId(BodyHandle handle) noexcept { return JPH::BodyID(handle.Value() - 1u); }

// Jolt calls this from inside the step, on whichever thread runs the collision job: it only
// records, under a lock. A body pair's contact begins with its first sub-shape contact and ends
// with its last.
class ContactBuffer final : public JPH::ContactListener {
  public:
    void OnContactAdded(const JPH::Body &a, const JPH::Body &b,
                        const JPH::ContactManifold &manifold,
                        JPH::ContactSettings &settings) override;
    void OnContactRemoved(const JPH::SubShapeIDPair &pair) override;

    void Take(std::vector<ContactFact> &out);
    // Owners of bodies, for the facts of contacts that end after a body is gone.
    void SetOwner(JPH::BodyID body, std::uint64_t owner);
    void ForgetOwner(JPH::BodyID body);

  private:
    using Pair = std::pair<std::uint32_t, std::uint32_t>;

    std::mutex m_Mutex;
    std::vector<ContactFact> m_Facts;
    std::map<Pair, std::uint32_t> m_Touching; // body pair → sub-shape contacts
    std::unordered_map<std::uint32_t, std::uint64_t> m_Owners;
};

struct NamedShape {
    std::string name;
    JPH::RefConst<JPH::Shape> shape;
};

struct WorldState {
    explicit WorldState(const PhysicsWorldSpec &spec);

    JoltRuntime runtime; // first: Jolt is set up before anything below uses it
    JoltLayers layers;
    JPH::TempAllocatorImpl scratch;
    JPH::JobSystemSingleThreaded jobs;
    ContactBuffer contacts;
    JPH::PhysicsSystem system;
    std::vector<NamedShape> shapes; // ShapeId n is shapes[n - 1]
    // Bodies removed since the last step: their owners are kept for the contacts that end in the
    // next step.
    std::vector<JPH::BodyID> removedSinceStep;
    std::uint64_t steps = 0;
};

} // namespace Runic::Physics::Detail
