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
//
// Characters (PHYS-2; RunicDocs: "Players and combat NPCs: CharacterVirtual (not a Body)") are
// upright capsules that move by collision checks, not forces: they walk on what is walkable, climb
// steps, stop at walls and slide along them, fall, and ride what they stand on. Every Step updates
// every character, moved or not, before the bodies, so a moving platform carries whoever stands on
// it (RunicDocs: "CharacterVirtuals update every 30 Hz tick even without input").

namespace Runic::Physics {

inline constexpr std::uint32_t kDefaultMaxBodies = 65'536;
inline constexpr std::uint32_t kDefaultMaxBodyPairs = 65'536;
inline constexpr std::uint32_t kDefaultMaxContactConstraints = 16'384;
inline constexpr std::uint32_t kDefaultStepScratchBytes = 16u * 1024u * 1024u;
inline constexpr float kStandardGravityMps2 = 9.81f;
inline constexpr float kDefaultFriction = 0.5f;
inline constexpr float kDefaultRestitution = 0.0f;
// A character's defaults: a person-sized capsule, and Jolt's own slope, step and floor values.
// Movement values are the caller's (the Engine's movement attributes, the Game's data); these only
// make a desc usable as it is.
inline constexpr float kDefaultCharacterRadiusM = 0.3f;
inline constexpr float kDefaultCharacterHeightM = 1.8f;
inline constexpr float kDefaultMaxSlopeDeg = 50.0f;
inline constexpr float kDefaultStepHeightM = 0.4f;
inline constexpr float kDefaultStickToFloorM = 0.5f;
// What counts as ground a character can stand on: slopes are measured from +Y, the world's up.
inline constexpr float kMaxSlopeLimitDeg = 90.0f;

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

// A character: an upright capsule whose pose is its feet (the bottom of the capsule) and its
// facing.
struct CharacterDesc {
    float radius = kDefaultCharacterRadiusM;
    float height = kDefaultCharacterHeightM; // feet to the top of the head, both caps included
    CollisionLayer layer;
    Pose pose; // the feet; the rotation turns the character about the up axis
    // The steepest slope it stands and walks on; steeper ground makes it slide.
    float maxSlopeDeg = kDefaultMaxSlopeDeg;
    // The highest step it climbs while walking (0: none).
    float stepHeight = kDefaultStepHeightM;
    // How far down it follows the ground while walking, so it stays on stairs and slopes going down
    // instead of flying off them (0: never).
    float stickToFloor = kDefaultStickToFloorM;
    std::uint64_t owner = 0; // handed back as the character's owner, like a body's
};

// What a character stands on after the last step.
enum class GroundState : std::uint8_t {
    OnGround,      // walkable ground
    OnSteepGround, // ground steeper than its maxSlopeDeg: it slides
    NotSupported,  // it touches something that cannot hold it (a wall): it falls
    InAir,         // it touches nothing below it
};

// What a character does in the next step, and only that one: a character nobody moves stands
// still, and still falls and rides what it stands on.
struct CharacterMove {
    // The velocity it walks at, in m/s (world space). Only the horizontal part counts, and only
    // while it stands on something: in the air it keeps the horizontal velocity it had.
    Core::Vec3 velocity{0.0f};
    // An upward speed in m/s it leaves walkable ground with (a jump; 0: none). Ignored in the air.
    float jumpSpeed = 0.0f;
};

struct CharacterState {
    Pose pose;
    Core::Vec3 velocity{0.0f}; // as it moved in the last step, the ground's motion included
    GroundState ground = GroundState::InAir;
    Core::Vec3 groundNormal{0.0f};
    Core::Vec3 groundVelocity{0.0f};
    BodyHandle groundBody; // none when it stands on nothing or on another character
    std::uint64_t groundOwner = 0;
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
    std::uint32_t characters = 0;
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

    // ---- Characters
    // -----------------------------------------------------------------------------------------
    // Fails when the capsule is degenerate (a height not above its two caps), or the slope, step
    // or floor values are out of range. A layer the world does not have is a programming error.
    Core::Result<CharacterHandle> CreateCharacter(const CharacterDesc &desc);
    // A handle that names no character is ignored.
    void RemoveCharacter(CharacterHandle character);
    bool IsCharacter(CharacterHandle character) const noexcept;
    // The move it makes in the next step (see CharacterMove).
    void MoveCharacter(CharacterHandle character, const CharacterMove &move);
    // Puts it somewhere at once (a teleport, a respawn, a turn), keeping its velocity.
    void SetCharacterPose(CharacterHandle character, const Pose &pose);
    CharacterState Character(CharacterHandle character) const;

    // ---- The step
    // -------------------------------------------------------------------------------------------
    // Advances the world by dtSeconds, once: every character in the order they were made (a removed
    // character's place is reused), then the bodies. Called by one thread.
    void Step(float dtSeconds);
    // The contacts that began or ended since the last call, in the order Jolt reported them.
    void TakeContacts(std::vector<ContactFact> &out);

    PhysicsStats Stats() const;

  private:
    struct State;
    std::unique_ptr<State> m_State;
};

} // namespace Runic::Physics
