#pragma once

#include <Jolt/Jolt.h>

#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyActivationListener.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Collision/ContactListener.h>
#include <Jolt/Physics/Collision/Shape/Shape.h>
#include <Jolt/Physics/PhysicsSystem.h>

#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "Characters/CharacterSet.hpp"
#include "CollisionLayers/JoltLayers.hpp"
#include "DebugExtraction/DebugCapture.hpp"
#include "JobSystem/PoolJobSystem.hpp"
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
//
// Jolt also removes every contact of a body that falls asleep, though it still touches; a sleeping
// pair is not a contact that ended. So a pair whose last sub-shape contact went is decided after
// the step (Settle): when its moving bodies are all asleep it stays touching, dormant; otherwise
// it ended. A dormant pair wakes silently when Jolt finds it again, and ends when one of its bodies
// woke up a step ago and Jolt did not, or when one of its bodies is removed.
//
// On the worker pool (PHYS-4) the callbacks come from several threads in an order that changes
// from run to run, though what Jolt finds does not. Jolt finds every sub-shape contact of a body
// pair in one go, on one thread and in a fixed order, so a pair's first contact (its point) is the
// same on any number of workers; across pairs, the pairs a step lost are decided in pair order and
// the step's facts are sorted by their bodies at its end, so the facts come out the same, in the
// same order.
class ContactBuffer final : public JPH::ContactListener, public JPH::BodyActivationListener {
  public:
    void OnContactAdded(const JPH::Body &a, const JPH::Body &b,
                        const JPH::ContactManifold &manifold,
                        JPH::ContactSettings &settings) override;
    void OnContactRemoved(const JPH::SubShapeIDPair &pair) override;
    void OnBodyActivated(const JPH::BodyID &body, JPH::uint64 userData) override;
    void OnBodyDeactivated(const JPH::BodyID &, JPH::uint64) override {}

    // Before the step, on the thread that steps: the facts from here on are the step's.
    void BeginStep();
    // After the step, on the thread that stepped: decides the pairs whose contacts went, then
    // sorts the step's facts.
    void Settle(const JPH::BodyInterface &bodies);
    // A body is being removed: its dormant pairs end now (Jolt reports only its awake contacts).
    void BodyRemoved(JPH::BodyID body);

    void Take(std::vector<ContactFact> &out);
    // Owners of bodies, for the facts of contacts that end after a body is gone.
    void SetOwner(JPH::BodyID body, std::uint64_t owner);
    void ForgetOwner(JPH::BodyID body);

  private:
    using Pair = std::pair<std::uint32_t, std::uint32_t>;

    void EndLocked(Pair pair);
    void ForgetDormantLocked(Pair pair);

    std::mutex m_Mutex;
    std::vector<ContactFact> m_Facts;
    std::map<Pair, std::uint32_t> m_Touching; // body pair → sub-shape contacts
    std::vector<Pair> m_Lost;                 // pairs whose last contact went in this step
    std::set<Pair> m_Dormant;                 // touching while their moving bodies sleep
    std::unordered_map<std::uint32_t, std::vector<Pair>> m_DormantByBody;
    std::vector<std::uint32_t> m_Woken;     // bodies woken since the last Settle
    std::vector<std::uint32_t> m_WokenLast; // bodies woken in the step before
    std::unordered_map<std::uint32_t, std::uint64_t> m_Owners;
    std::size_t m_StepFirstFact = 0; // the first of m_Facts made in this step
};

struct NamedShape {
    std::string name;
    ShapeKind kind = ShapeKind::Box;
    JPH::RefConst<JPH::Shape> shape; // its user data is its ShapeId
};

struct WorldState {
    explicit WorldState(const PhysicsWorldSpec &spec);

    JoltRuntime runtime; // first: Jolt is set up before anything below uses it
    JoltLayers layers;
    JPH::TempAllocatorImpl scratch;
    PoolJobSystem jobs; // Jolt on the worker pool (PHYS-4)
    ContactBuffer contacts;
    JPH::PhysicsSystem system;
    CharacterSet characters;        // after the system its characters belong to
    std::vector<NamedShape> shapes; // ShapeId n is shapes[n - 1]
    // Bodies removed since the last step: their owners are kept for the contacts that end in the
    // next step.
    std::vector<JPH::BodyID> removedSinceStep;
    std::uint64_t steps = 0;
    // The debug view (PHYS-3): Jolt's renderer, made at the first extraction, and its scratch.
    DebugCapturePtr debug;
    std::vector<JPH::BodyID> debugBodies;
};

} // namespace Runic::Physics::Detail

namespace Runic::Physics {

// The PhysicsWorld's private state, defined here so every file of the module that implements the
// world (PhysicsWorld/, Characters/) reaches it.
struct PhysicsWorld::State : Detail::WorldState {
    using WorldState::WorldState;
};

} // namespace Runic::Physics
