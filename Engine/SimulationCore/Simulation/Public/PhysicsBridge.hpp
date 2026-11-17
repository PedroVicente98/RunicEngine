#pragma once

#include <cstdint>
#include <memory>
#include <string_view>

#include "Entity.hpp"
#include "EventBuffer.hpp"
#include "Math.hpp"
#include "PhysicsWorld.hpp"
#include "WorldSpace.hpp"

// The physics bridge (SIM-8; RunicDocs › Simulation core › Flecs and Jolt together): the entities
// hold the truth, Physics holds the bodies and characters behind opaque handles in components.
//
//   - Bodies and characters follow their entities. AddBody, AddCharacter and Remove are requests:
//     the PhysicsStructure phase applies them in batches, so an entity has its body from the
//     PhysicsStep phase of the tick it was asked for (between steps or in an earlier phase) on.
//     Destroying an entity is enough to take its body: it goes at the next barrier, so the
//     entities and Physics never disagree inside a phase.
//   - A sync on each side of the step. In the PhysicsStep phase the bridge moves each kinematic
//     body to its entity's WorldTransform and hands each character its move, then steps the
//     physics world once; in the PostPhysics phase it writes dynamic bodies' poses and characters'
//     positions back into their WorldTransforms.
//   - Contacts are facts. Jolt reports them inside the step and Physics only buffers them; the
//     bridge emits them as PhysicsContact facts in PostPhysics, readable from the Queries phase on.
//     Nothing Jolt calls touches the entities.
//
// The bridge's systems are the first of their phases when physics is enabled before other systems
// of those phases are added: a system that moves kinematic entities or characters runs in the
// Movement phase, and one that reads where physics put things runs after PostPhysics' write-back.

namespace Runic::Simulation {

// An entity's body: the bridge adds it and keeps it; the handle names the body in the bridge's
// physics world.
struct PhysicsBody {
    Physics::BodyHandle handle;
    Physics::MotionType motion = Physics::MotionType::Static;
};

// An entity's character. The entity's WorldTransform position is the character's feet after every
// step; its rotation is the entity's own (an upright capsule turns nothing in physics).
struct PhysicsCharacter {
    Physics::CharacterHandle handle;
    // The move the character makes in the next physics step, written in the Movement phase. The
    // bridge hands it over and clears it: a character nobody moves stands.
    Physics::CharacterMove move;
};

// A contact between two bodies, as a fact: the entities they belong to (the null entity for a
// body no entity owns), and where they touched when they began to (point, and the normal from a
// to b).
struct PhysicsContact {
    Physics::ContactPhase phase = Physics::ContactPhase::Began;
    ECS::Entity a;
    ECS::Entity b;
    Core::WorldPosition point;
    Core::Vec3 normal{0.0f};
};

// The names the components and the facts' buffer are registered under.
inline constexpr std::string_view kPhysicsBodyName = "engine:physics_body";
inline constexpr std::string_view kPhysicsCharacterName = "engine:physics_character";
inline constexpr std::string_view kPhysicsContactsBufferName = "engine:physics_contacts";
// The bridge's systems, for metrics and logs.
inline constexpr std::string_view kPhysicsStructureSystemName = "engine:physics_structure";
inline constexpr std::string_view kPhysicsStepSystemName = "engine:physics_step";
inline constexpr std::string_view kPhysicsWriteBackSystemName = "engine:physics_write_back";

struct PhysicsBridgeStats {
    std::uint32_t bodies = 0;     // entities with a body
    std::uint32_t characters = 0; // entities with a character
    std::uint32_t pending = 0;    // requests waiting for the next PhysicsStructure phase
    // Bodies and characters taken at a barrier because their entity was destroyed or lost its
    // component.
    std::uint64_t followedDestroyed = 0;
};

class SimulationRuntime;

class PhysicsBridge {
  public:
    // Made by SimulationRuntime::EnablePhysics, which owns it.
    PhysicsBridge(SimulationRuntime &runtime, const Physics::PhysicsWorldSpec &spec);
    ~PhysicsBridge();
    PhysicsBridge(const PhysicsBridge &) = delete;
    PhysicsBridge &operator=(const PhysicsBridge &) = delete;

    // ---- Requests (the simulation's thread; between steps or in any phase) --------------------
    // The entity gets a body at the next PhysicsStructure phase, at its WorldTransform (it gets
    // one from desc.pose if it has none), owned by the entity (desc.owner is ignored). An entity
    // has one body or one character.
    void AddBody(ECS::Entity entity, const Physics::BodyDesc &desc);
    // The same for a character; a description Physics refuses is logged and dropped.
    void AddCharacter(ECS::Entity entity, const Physics::CharacterDesc &desc);
    // The entity's body or character goes at the next PhysicsStructure phase, with its component
    // (a request still waiting is dropped instead).
    void Remove(ECS::Entity entity);

    // The physics world: its shapes and layers to set up, and poses to read. The bodies and
    // characters of entities are the bridge's: change them through entities and requests.
    Physics::PhysicsWorld &World() noexcept;
    const Physics::PhysicsWorld &World() const noexcept;
    // The contact facts.
    Events::EventBuffer<PhysicsContact> &Contacts() noexcept;

    PhysicsBridgeStats Stats() const noexcept;

  private:
    friend class SimulationRuntime;

    // The runtime calls this at every barrier, after held-back changes applied: the bodies and
    // characters of entities destroyed (or that lost their component) go.
    void AtBarrier();

    struct State;
    std::unique_ptr<State> m_State;
};

} // namespace Runic::Simulation
