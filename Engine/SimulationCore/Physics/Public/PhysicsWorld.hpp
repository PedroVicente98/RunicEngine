#pragma once

#include <cstdint>
#include <memory>
#include <span>
#include <string_view>
#include <vector>

#include "CollisionLayers.hpp"
#include "Math.hpp"
#include "PhysicsHandles.hpp"
#include "PhysicsShapes.hpp"
#include "Result.hpp"
#include "WorldSpace.hpp"

// The PhysicsWorld (PHYS-1; RunicDocs › Simulation core › Jolt rules): Jolt v5.6 in double
// precision behind Runic types. Bodies sit at 64-bit world positions (Core::WorldPosition), so a
// crate 6,000 km from the origin collides as exactly as one at it; shapes are 32-bit, relative to
// their body. Jolt stays private: no Jolt type appears here, and Jolt creates no thread (it runs on
// the caller's thread until PHYS-4 puts it on the Core worker pool).
//
// Creating and removing bodies are structural changes: the Simulation's bridge does them in batches
// at the 30 Hz boundary (CreateBodies, RemoveBodies). Contacts are facts: Jolt reports them from
// inside Step, and the world only buffers them; TakeContacts hands them over afterwards.

namespace Runic::Physics {

inline constexpr std::uint32_t kDefaultMaxBodies = 65'536;
inline constexpr std::uint32_t kDefaultMaxBodyPairs = 65'536;
inline constexpr std::uint32_t kDefaultMaxContactConstraints = 16'384;
inline constexpr std::uint32_t kDefaultStepScratchBytes = 16u * 1024u * 1024u;
inline constexpr float kStandardGravityMps2 = 9.81f;
inline constexpr float kDefaultFriction = 0.5f;
inline constexpr float kDefaultRestitution = 0.0f;

struct PhysicsWorldSpec {
    std::uint32_t maxBodies = kDefaultMaxBodies;
    std::uint32_t maxBodyPairs = kDefaultMaxBodyPairs;
    std::uint32_t maxContactConstraints = kDefaultMaxContactConstraints;
    // Scratch memory one step may use, allocated once.
    std::uint32_t stepScratchBytes = kDefaultStepScratchBytes;
    Core::Vec3 gravity{0.0f, -kStandardGravityMps2, 0.0f};
    CollisionLayers layers = DefaultCollisionLayers();
};

// Where a body or a character is.
struct Pose {
    Core::WorldPosition position;
    Core::Quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
};

struct BodyDesc {
    ShapeId shape;
    MotionType motion = MotionType::Static;
    CollisionLayer layer;
    Pose pose;
    Core::Vec3 linearVelocity{0.0f};
    float friction = kDefaultFriction;
    float restitution = kDefaultRestitution;
    bool sensor = false; // reports overlaps, pushes nothing
    // Whoever owns the body (the Simulation's bridge: its entity), handed back in contacts.
    std::uint64_t owner = 0;
};

enum class ContactPhase : std::uint8_t { Began, Ended };

// A contact between two bodies, as a fact. point and normal are given when it began (the normal
// points from a to b).
struct ContactFact {
    ContactPhase phase = ContactPhase::Began;
    BodyHandle a;
    BodyHandle b;
    std::uint64_t ownerA = 0;
    std::uint64_t ownerB = 0;
    Core::WorldPosition point;
    Core::Vec3 normal{0.0f};
};

struct PhysicsStats {
    std::uint32_t bodies = 0;
    std::uint32_t activeBodies = 0;
    std::uint32_t shapes = 0;
    std::uint64_t steps = 0;
};

class PhysicsWorld {
  public:
    explicit PhysicsWorld(const PhysicsWorldSpec &spec = {});
    ~PhysicsWorld();
    PhysicsWorld(const PhysicsWorld &) = delete;
    PhysicsWorld &operator=(const PhysicsWorld &) = delete;

    const CollisionLayers &Layers() const noexcept;

    // ---- Shapes
    // ---------------------------------------------------------------------------------------------
    // Adds a shape under a name ("namespace:name", unique). Fails on a degenerate description.
    Core::Result<ShapeId> AddShape(std::string_view name, const ShapeDesc &desc);
    ShapeId FindShape(std::string_view name) const noexcept;

    // ---- Bodies
    // ---------------------------------------------------------------------------------------------
    // Creates bodies in one batch (Jolt's AddBodiesPrepare and AddBodiesFinalize): out[i] is
    // descs[i]'s body.
    void CreateBodies(std::span<const BodyDesc> descs, std::span<BodyHandle> out);
    BodyHandle CreateBody(const BodyDesc &desc);
    // Removes and destroys bodies in one batch. A handle that names no body is ignored.
    void RemoveBodies(std::span<const BodyHandle> bodies);
    bool IsBody(BodyHandle body) const noexcept;

    Pose BodyPose(BodyHandle body) const;
    Core::Vec3 LinearVelocity(BodyHandle body) const;
    bool IsActive(BodyHandle body) const;
    // A kinematic body reaches pose at the end of the next step of dtSeconds, pushing what is in
    // its way.
    void MoveKinematic(BodyHandle body, const Pose &pose, float dtSeconds);
    void SetLinearVelocity(BodyHandle body, Core::Vec3 velocity);

    // ---- The step
    // -------------------------------------------------------------------------------------------
    // Advances the world by dtSeconds, once. Called by one thread.
    void Step(float dtSeconds);
    // The contacts that began or ended since the last call, in the order Jolt reported them.
    void TakeContacts(std::vector<ContactFact> &out);

    PhysicsStats Stats() const;

  private:
    struct State;
    std::unique_ptr<State> m_State;
};

} // namespace Runic::Physics
