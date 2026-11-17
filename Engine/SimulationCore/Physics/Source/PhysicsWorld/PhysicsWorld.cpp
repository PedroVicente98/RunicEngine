// The PhysicsWorld (PhysicsWorld.hpp) on Jolt: shapes, bodies in batches, the step and its
// contacts.

#include "PhysicsWorld.hpp"

// Jolt.h comes before any other Jolt header.
#include <Jolt/Jolt.h>

#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>

#include <algorithm>
#include <cmath>
#include <format>

#include "Assert.hpp"
#include "PhysicsWorld/WorldState.hpp"

namespace Runic::Physics {

namespace Detail {

namespace {

// Jolt's job system needs to know how many jobs and barriers a step may make; its samples' values.
constexpr std::uint32_t kMaxPhysicsJobs = JPH::cMaxPhysicsJobs;
// Body mutexes: 0 lets Jolt pick a default for the machine.
constexpr std::uint32_t kDefaultBodyMutexes = 0;
// Jolt is most stable with a collision step every 1/60 s: two per 30 Hz tick.
constexpr float kCollisionStepsPerSecond = 60.0f;
constexpr int kMinCollisionSteps = 1;
// A box's rounded edges: Jolt's default, smaller for small boxes.
constexpr float kBoxConvexRadiusM = 0.05f;
constexpr std::uint32_t kIndicesPerTriangle = 3;
constexpr std::uint32_t kFirstContactPoint = 0;
constexpr char kNamespaceSeparator = ':';

} // namespace

void ContactBuffer::OnContactAdded(const JPH::Body &a, const JPH::Body &b,
                                   const JPH::ContactManifold &manifold, JPH::ContactSettings &) {
    const std::lock_guard lock(m_Mutex);
    const Pair pair{a.GetID().GetIndexAndSequenceNumber(), b.GetID().GetIndexAndSequenceNumber()};
    if (m_Touching[pair]++ > 0) {
        return; // another sub-shape of a pair that already touches
    }
    ContactFact fact;
    fact.phase = ContactPhase::Began;
    fact.a = ToHandle(a.GetID());
    fact.b = ToHandle(b.GetID());
    fact.ownerA = a.GetUserData();
    fact.ownerB = b.GetUserData();
    fact.point = FromJolt(manifold.GetWorldSpaceContactPointOn1(kFirstContactPoint));
    fact.normal = FromJolt(manifold.mWorldSpaceNormal);
    m_Facts.push_back(fact);
}

void ContactBuffer::OnContactRemoved(const JPH::SubShapeIDPair &pair) {
    const std::lock_guard lock(m_Mutex);
    const std::uint32_t first = pair.GetBody1ID().GetIndexAndSequenceNumber();
    const std::uint32_t second = pair.GetBody2ID().GetIndexAndSequenceNumber();
    const auto touching = m_Touching.find({first, second});
    if (touching == m_Touching.end() || --touching->second > 0) {
        return;
    }
    m_Touching.erase(touching);
    ContactFact fact;
    fact.phase = ContactPhase::Ended;
    fact.a = ToHandle(pair.GetBody1ID());
    fact.b = ToHandle(pair.GetBody2ID());
    const auto ownerA = m_Owners.find(first);
    const auto ownerB = m_Owners.find(second);
    fact.ownerA = ownerA == m_Owners.end() ? 0 : ownerA->second;
    fact.ownerB = ownerB == m_Owners.end() ? 0 : ownerB->second;
    m_Facts.push_back(fact);
}

void ContactBuffer::Take(std::vector<ContactFact> &out) {
    const std::lock_guard lock(m_Mutex);
    out.insert(out.end(), m_Facts.begin(), m_Facts.end());
    m_Facts.clear();
}

void ContactBuffer::SetOwner(JPH::BodyID body, std::uint64_t owner) {
    const std::lock_guard lock(m_Mutex);
    m_Owners[body.GetIndexAndSequenceNumber()] = owner;
}

void ContactBuffer::ForgetOwner(JPH::BodyID body) {
    const std::lock_guard lock(m_Mutex);
    m_Owners.erase(body.GetIndexAndSequenceNumber());
}

WorldState::WorldState(const PhysicsWorldSpec &spec)
    : layers(spec.layers), scratch(spec.stepScratchBytes), jobs(kMaxPhysicsJobs) {
    system.Init(spec.maxBodies, kDefaultBodyMutexes, spec.maxBodyPairs, spec.maxContactConstraints,
                layers, layers, layers);
    system.SetGravity(ToJolt(spec.gravity));
    system.SetContactListener(&contacts);
}

namespace {

Core::Result<JPH::RefConst<JPH::Shape>> MakeShape(const ShapeDesc &desc) {
    switch (desc.kind) {
    case ShapeKind::Box: {
        const Core::Vec3 half = desc.halfExtents;
        const float smallest = std::min({half.x, half.y, half.z});
        if (smallest < kMinShapeSizeM) {
            return Core::Fail("a box needs half extents of at least 1 mm");
        }
        const float convex = std::min(kBoxConvexRadiusM, smallest);
        return JPH::RefConst<JPH::Shape>(new JPH::BoxShape(ToJolt(half), convex));
    }
    case ShapeKind::Sphere:
        if (desc.radius < kMinShapeSizeM) {
            return Core::Fail("a sphere needs a radius of at least 1 mm");
        }
        return JPH::RefConst<JPH::Shape>(new JPH::SphereShape(desc.radius));
    case ShapeKind::Capsule:
        if (desc.radius < kMinShapeSizeM || desc.halfHeight < kMinShapeSizeM) {
            return Core::Fail("a capsule needs a radius and a half height of at least 1 mm");
        }
        return JPH::RefConst<JPH::Shape>(new JPH::CapsuleShape(desc.halfHeight, desc.radius));
    case ShapeKind::Mesh: {
        if (desc.indices.empty() || desc.indices.size() % kIndicesPerTriangle != 0) {
            return Core::Fail("a mesh needs whole triangles, three indices each");
        }
        JPH::VertexList vertices;
        vertices.reserve(desc.vertices.size());
        for (const Core::Vec3 &vertex : desc.vertices) {
            vertices.push_back(JPH::Float3(vertex.x, vertex.y, vertex.z));
        }
        JPH::IndexedTriangleList triangles;
        triangles.reserve(desc.indices.size() / kIndicesPerTriangle);
        for (std::size_t i = 0; i < desc.indices.size(); i += kIndicesPerTriangle) {
            const std::uint32_t a = desc.indices[i];
            const std::uint32_t b = desc.indices[i + 1];
            const std::uint32_t c = desc.indices[i + 2];
            if (a >= vertices.size() || b >= vertices.size() || c >= vertices.size()) {
                return Core::Fail(std::format("triangle {} names a vertex the mesh does not have",
                                              i / kIndicesPerTriangle));
            }
            triangles.push_back(JPH::IndexedTriangle(a, b, c));
        }
        const JPH::MeshShapeSettings settings(std::move(vertices), std::move(triangles));
        const JPH::ShapeSettings::ShapeResult result = settings.Create();
        if (result.HasError()) {
            return Core::Fail(
                std::format("the mesh is not a valid shape: {}", result.GetError().c_str()));
        }
        return JPH::RefConst<JPH::Shape>(result.Get());
    }
    }
    return Core::Fail("an unknown shape kind");
}

JPH::EMotionType MotionOf(MotionType motion) {
    switch (motion) {
    case MotionType::Static:
        return JPH::EMotionType::Static;
    case MotionType::Kinematic:
        return JPH::EMotionType::Kinematic;
    case MotionType::Dynamic:
        return JPH::EMotionType::Dynamic;
    }
    return JPH::EMotionType::Static;
}

} // namespace

} // namespace Detail

struct PhysicsWorld::State : Detail::WorldState {
    using WorldState::WorldState;
};

PhysicsWorld::PhysicsWorld(const PhysicsWorldSpec &spec) : m_State(std::make_unique<State>(spec)) {
    RUNIC_ASSERT_MSG(spec.layers.Count() > 0, "a PhysicsWorld needs collision layers");
}

PhysicsWorld::~PhysicsWorld() {
    // Every body goes before the system that holds it.
    JPH::BodyIDVector bodies;
    m_State->system.GetBodies(bodies);
    JPH::BodyInterface &interface = m_State->system.GetBodyInterfaceNoLock();
    if (!bodies.empty()) {
        interface.RemoveBodies(bodies.data(), static_cast<int>(bodies.size()));
        interface.DestroyBodies(bodies.data(), static_cast<int>(bodies.size()));
    }
}

const CollisionLayers &PhysicsWorld::Layers() const noexcept { return m_State->layers.Table(); }

Core::Result<ShapeId> PhysicsWorld::AddShape(std::string_view name, const ShapeDesc &desc) {
    if (name.find(Detail::kNamespaceSeparator) == std::string_view::npos) {
        return Core::Fail(std::format("the shape name '{}' is not namespace:name", name));
    }
    if (FindShape(name).IsValid()) {
        return Core::Fail(std::format("a shape is already called '{}'", name));
    }
    auto shape = Detail::MakeShape(desc);
    if (!shape) {
        return Core::Fail(std::format("shape '{}': {}", name, shape.GetError().message));
    }
    m_State->shapes.push_back({std::string(name), shape.Value()});
    return ShapeId(static_cast<std::uint32_t>(m_State->shapes.size()));
}

ShapeId PhysicsWorld::FindShape(std::string_view name) const noexcept {
    for (std::size_t i = 0; i < m_State->shapes.size(); ++i) {
        if (m_State->shapes[i].name == name) {
            return ShapeId(static_cast<std::uint32_t>(i + 1));
        }
    }
    return ShapeId();
}

void PhysicsWorld::CreateBodies(std::span<const BodyDesc> descs, std::span<BodyHandle> out) {
    RUNIC_ASSERT(out.size() >= descs.size());
    State &state = *m_State;
    JPH::BodyInterface &interface = state.system.GetBodyInterface();
    std::vector<JPH::BodyID> created;
    created.reserve(descs.size());
    for (std::size_t i = 0; i < descs.size(); ++i) {
        const BodyDesc &desc = descs[i];
        RUNIC_ASSERT_MSG(desc.shape.IsValid() && desc.shape.Value() <= state.shapes.size(),
                         "a body names a shape the world does not have");
        RUNIC_ASSERT_MSG(desc.layer.IsValid() && desc.layer.Value() <= state.layers.Table().Count(),
                         "a body names a collision layer the world does not have");
        JPH::BodyCreationSettings settings(state.shapes[desc.shape.Value() - 1u].shape,
                                           Detail::ToJolt(desc.pose.position),
                                           Detail::ToJolt(desc.pose.rotation),
                                           Detail::MotionOf(desc.motion), Detail::ToJolt(desc.layer));
        settings.mLinearVelocity = Detail::ToJolt(desc.linearVelocity);
        settings.mFriction = desc.friction;
        settings.mRestitution = desc.restitution;
        settings.mIsSensor = desc.sensor;
        settings.mUserData = desc.owner;
        JPH::Body *body = interface.CreateBody(settings);
        RUNIC_ASSERT_MSG(body != nullptr,
                         "the PhysicsWorld is full: raise PhysicsWorldSpec::maxBodies");
        if (body == nullptr) {
            out[i] = BodyHandle();
            continue;
        }
        state.contacts.SetOwner(body->GetID(), desc.owner);
        created.push_back(body->GetID());
        out[i] = Detail::ToHandle(body->GetID());
    }
    if (created.empty()) {
        return;
    }
    const auto count = static_cast<int>(created.size());
    JPH::BodyInterface::AddState add = interface.AddBodiesPrepare(created.data(), count);
    interface.AddBodiesFinalize(created.data(), count, add, JPH::EActivation::Activate);
}

BodyHandle PhysicsWorld::CreateBody(const BodyDesc &desc) {
    BodyHandle handle;
    CreateBodies(std::span(&desc, 1), std::span(&handle, 1));
    return handle;
}

void PhysicsWorld::RemoveBodies(std::span<const BodyHandle> bodies) {
    State &state = *m_State;
    JPH::BodyInterface &interface = state.system.GetBodyInterface();
    std::vector<JPH::BodyID> removed;
    removed.reserve(bodies.size());
    for (const BodyHandle handle : bodies) {
        if (IsBody(handle)) {
            removed.push_back(Detail::ToBodyId(handle));
        }
    }
    if (removed.empty()) {
        return;
    }
    const auto count = static_cast<int>(removed.size());
    interface.RemoveBodies(removed.data(), count);
    interface.DestroyBodies(removed.data(), count);
    // Contacts of removed bodies end at the next step; their owners are remembered until then.
    state.removedSinceStep.insert(state.removedSinceStep.end(), removed.begin(), removed.end());
}

bool PhysicsWorld::IsBody(BodyHandle body) const noexcept {
    return body.IsValid() && m_State->system.GetBodyInterface().IsAdded(Detail::ToBodyId(body));
}

Pose PhysicsWorld::BodyPose(BodyHandle body) const {
    RUNIC_ASSERT(IsBody(body));
    JPH::RVec3 position;
    JPH::Quat rotation;
    m_State->system.GetBodyInterface().GetPositionAndRotation(Detail::ToBodyId(body), position,
                                                              rotation);
    return {Detail::FromJolt(position), Detail::FromJolt(rotation)};
}

Core::Vec3 PhysicsWorld::LinearVelocity(BodyHandle body) const {
    RUNIC_ASSERT(IsBody(body));
    return Detail::FromJolt(
        m_State->system.GetBodyInterface().GetLinearVelocity(Detail::ToBodyId(body)));
}

bool PhysicsWorld::IsActive(BodyHandle body) const {
    RUNIC_ASSERT(IsBody(body));
    return m_State->system.GetBodyInterface().IsActive(Detail::ToBodyId(body));
}

void PhysicsWorld::MoveKinematic(BodyHandle body, const Pose &pose, float dtSeconds) {
    RUNIC_ASSERT(IsBody(body));
    m_State->system.GetBodyInterface().MoveKinematic(Detail::ToBodyId(body),
                                                     Detail::ToJolt(pose.position),
                                                     Detail::ToJolt(pose.rotation), dtSeconds);
}

void PhysicsWorld::SetLinearVelocity(BodyHandle body, Core::Vec3 velocity) {
    RUNIC_ASSERT(IsBody(body));
    m_State->system.GetBodyInterface().SetLinearVelocity(Detail::ToBodyId(body),
                                                         Detail::ToJolt(velocity));
}

void PhysicsWorld::Step(float dtSeconds) {
    RUNIC_ASSERT(dtSeconds > 0.0f);
    State &state = *m_State;
    const int collisionSteps =
        std::max(Detail::kMinCollisionSteps,
                 static_cast<int>(std::lround(dtSeconds * Detail::kCollisionStepsPerSecond)));
    state.system.Update(dtSeconds, collisionSteps, &state.scratch, &state.jobs);
    ++state.steps;
    for (const JPH::BodyID removed : state.removedSinceStep) {
        state.contacts.ForgetOwner(removed);
    }
    state.removedSinceStep.clear();
}

void PhysicsWorld::TakeContacts(std::vector<ContactFact> &out) { m_State->contacts.Take(out); }

PhysicsStats PhysicsWorld::Stats() const {
    PhysicsStats stats;
    stats.bodies = m_State->system.GetNumBodies();
    stats.activeBodies = m_State->system.GetNumActiveBodies(JPH::EBodyType::RigidBody);
    stats.shapes = static_cast<std::uint32_t>(m_State->shapes.size());
    stats.steps = m_State->steps;
    return stats;
}

} // namespace Runic::Physics
