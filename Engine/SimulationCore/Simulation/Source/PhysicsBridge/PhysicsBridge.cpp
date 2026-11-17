// The physics bridge (PhysicsBridge.hpp): requests applied in the PhysicsStructure phase, the sync
// on each side of the step, contacts as facts, and bodies that follow their entities at barriers.

#include "PhysicsBridge.hpp"

#include <algorithm>
#include <format>
#include <string>
#include <unordered_map>
#include <vector>

#include "Assert.hpp"
#include "EntityQuery.hpp"
#include "EntityWorld.hpp"
#include "Log.hpp"
#include "Scheduler/RuntimeState.hpp"
#include "SimulationRuntime.hpp"
#include "WorldComponents.hpp"

namespace Runic::Simulation {

namespace {

constexpr std::string_view kPhysicsBodyDescription =
    "The entity's body in the simulation's physics world (opaque handle) and how it moves";
constexpr std::string_view kPhysicsCharacterDescription =
    "The entity's character in the simulation's physics world (opaque handle) and its next move";
constexpr std::string_view kKinematicSyncQuery = "engine:physics_kinematic_targets";
constexpr std::string_view kBodyWriteBackQuery = "engine:physics_body_write_back";
constexpr std::string_view kCharacterMoveQuery = "engine:physics_character_moves";
constexpr std::string_view kCharacterWriteBackQuery = "engine:physics_character_write_back";
constexpr std::string_view kBodiesPresentQuery = "engine:physics_bodies";
constexpr std::string_view kCharactersPresentQuery = "engine:physics_characters";

std::string_view MotionName(Physics::MotionType motion) {
    switch (motion) {
    case Physics::MotionType::Static:
        return "static";
    case Physics::MotionType::Kinematic:
        return "kinematic";
    case Physics::MotionType::Dynamic:
        return "dynamic";
    }
    return "static";
}

void FormatBody(const void *component, std::string &out) {
    const auto &body = *static_cast<const PhysicsBody *>(component);
    out = std::format("body {} ({})", body.handle.Value(), MotionName(body.motion));
}

void FormatCharacter(const void *component, std::string &out) {
    const auto &character = *static_cast<const PhysicsCharacter *>(component);
    out = std::format("character {}, move ({:.2f}, {:.2f}, {:.2f}) m/s", character.handle.Value(),
                      character.move.velocity.x, character.move.velocity.y,
                      character.move.velocity.z);
}

} // namespace

struct PhysicsBridge::State {
    State(SimulationRuntime &owner, const Physics::PhysicsWorldSpec &spec)
        : entities(owner.Entities()), world(spec),
          contacts(owner.EventHub(), kPhysicsContactsBufferName) {}

    // An entity the bridge gave a body or a character.
    struct Tracked {
        ECS::Entity entity;
        bool character = false;
        Physics::BodyHandle body;
        Physics::CharacterHandle characterHandle;
    };
    struct PendingBody {
        ECS::Entity entity;
        Physics::BodyDesc desc;
    };
    struct PendingCharacter {
        ECS::Entity entity;
        Physics::CharacterDesc desc;
    };

    ECS::EntityWorld &entities;
    Physics::PhysicsWorld world;
    Events::EventBuffer<PhysicsContact> contacts;

    ECS::EntityQuery<const PhysicsBody, const World::WorldTransform> kinematicTargets;
    ECS::EntityQuery<const PhysicsBody, World::WorldTransform> bodyWriteBack;
    ECS::EntityQuery<PhysicsCharacter> characterMoves;
    ECS::EntityQuery<const PhysicsCharacter, World::WorldTransform> characterWriteBack;
    ECS::EntityQuery<const PhysicsBody> bodiesPresent;
    ECS::EntityQuery<const PhysicsCharacter> charactersPresent;

    std::vector<Tracked> tracked;
    std::unordered_map<ECS::Entity, std::size_t> trackedIndex;
    std::vector<PendingBody> pendingBodies;
    std::vector<PendingCharacter> pendingCharacters;
    std::vector<ECS::Entity> pendingRemovals;
    std::uint32_t bodies = 0;
    std::uint32_t characters = 0;
    std::uint64_t followedDestroyed = 0;

    // Kept between ticks so the batches allocate only when they grow.
    std::vector<Physics::BodyDesc> descs;
    std::vector<Physics::BodyHandle> handles;
    std::vector<Physics::BodyHandle> leaving;
    std::vector<Physics::ContactFact> facts;

    bool IsPending(ECS::Entity entity) const;
    void Track(const Tracked &entry);
    // Takes the entry's body or character out of the physics world (bodies into `leaving`, for
    // one batch) and forgets it.
    void Untrack(std::size_t index);

    void ApplyRequests();
    void SyncBeforeStep(float dtSeconds);
    void WriteBack();
    void FollowDestroyed();
};

bool PhysicsBridge::State::IsPending(ECS::Entity entity) const {
    return std::ranges::any_of(pendingBodies,
                               [entity](const PendingBody &p) { return p.entity == entity; }) ||
           std::ranges::any_of(pendingCharacters,
                               [entity](const PendingCharacter &p) { return p.entity == entity; });
}

void PhysicsBridge::State::Track(const Tracked &entry) {
    trackedIndex[entry.entity] = tracked.size();
    tracked.push_back(entry);
    if (entry.character) {
        ++characters;
    } else {
        ++bodies;
    }
}

void PhysicsBridge::State::Untrack(std::size_t index) {
    const Tracked entry = tracked[index];
    if (entry.character) {
        world.RemoveCharacter(entry.characterHandle);
        --characters;
    } else {
        leaving.push_back(entry.body);
        --bodies;
    }
    trackedIndex.erase(entry.entity);
    if (index + 1 != tracked.size()) {
        tracked[index] = tracked.back();
        trackedIndex[tracked[index].entity] = index;
    }
    tracked.pop_back();
}

void PhysicsBridge::State::ApplyRequests() {
    // Removals first, so an entity removed and added again in one tick ends with its new body.
    leaving.clear();
    for (const ECS::Entity entity : pendingRemovals) {
        const auto found = trackedIndex.find(entity);
        if (found == trackedIndex.end()) {
            continue;
        }
        const bool character = tracked[found->second].character;
        Untrack(found->second);
        if (character) {
            entities.Remove<PhysicsCharacter>(entity);
        } else {
            entities.Remove<PhysicsBody>(entity);
        }
    }
    pendingRemovals.clear();
    world.RemoveBodies(leaving);

    // Bodies in one batch, at their entities' transforms.
    descs.clear();
    std::erase_if(pendingBodies,
                  [this](const PendingBody &p) { return !entities.IsAlive(p.entity); });
    for (PendingBody &pending : pendingBodies) {
        if (const auto *transform = entities.TryGet<World::WorldTransform>(pending.entity)) {
            pending.desc.pose = {transform->position, transform->rotation};
        } else {
            entities.Set(pending.entity, World::WorldTransform{pending.desc.pose.position,
                                                               pending.desc.pose.rotation});
        }
        pending.desc.owner = pending.entity.Raw();
        descs.push_back(pending.desc);
    }
    handles.resize(descs.size());
    world.CreateBodies(descs, handles);
    for (std::size_t i = 0; i < pendingBodies.size(); ++i) {
        const PendingBody &pending = pendingBodies[i];
        entities.Set(pending.entity, PhysicsBody{handles[i], pending.desc.motion});
        Tracked entry;
        entry.entity = pending.entity;
        entry.body = handles[i];
        Track(entry);
    }
    pendingBodies.clear();

    for (PendingCharacter &pending : pendingCharacters) {
        if (!entities.IsAlive(pending.entity)) {
            continue;
        }
        if (const auto *transform = entities.TryGet<World::WorldTransform>(pending.entity)) {
            pending.desc.pose = {transform->position, transform->rotation};
        } else {
            entities.Set(pending.entity, World::WorldTransform{pending.desc.pose.position,
                                                               pending.desc.pose.rotation});
        }
        pending.desc.owner = pending.entity.Raw();
        auto character = world.CreateCharacter(pending.desc);
        if (!character) {
            RUNIC_LOG_ERROR("RUNIC: entity {} gets no character: {}", pending.entity.Raw(),
                            character.GetError().message);
            continue;
        }
        entities.Set(pending.entity, PhysicsCharacter{character.Value(), {}});
        Tracked entry;
        entry.entity = pending.entity;
        entry.character = true;
        entry.characterHandle = character.Value();
        Track(entry);
    }
    pendingCharacters.clear();
}

void PhysicsBridge::State::SyncBeforeStep(float dtSeconds) {
    kinematicTargets.Each(
        [this, dtSeconds](ECS::Entity, const PhysicsBody &body, const World::WorldTransform &at) {
            if (body.motion == Physics::MotionType::Kinematic) {
                world.MoveKinematic(body.handle, {at.position, at.rotation}, dtSeconds);
            }
        });
    characterMoves.Each([this](ECS::Entity, PhysicsCharacter &character) {
        world.MoveCharacter(character.handle, character.move);
        character.move = {};
    });
    world.Step(dtSeconds);
}

void PhysicsBridge::State::WriteBack() {
    bodyWriteBack.Each([this](ECS::Entity, const PhysicsBody &body, World::WorldTransform &at) {
        if (body.motion == Physics::MotionType::Dynamic) {
            const Physics::Pose pose = world.BodyPose(body.handle);
            at.position = pose.position;
            at.rotation = pose.rotation;
        }
    });
    characterWriteBack.Each(
        [this](ECS::Entity, const PhysicsCharacter &character, World::WorldTransform &at) {
            at.position = world.Character(character.handle).pose.position;
        });
    facts.clear();
    world.TakeContacts(facts);
    for (const Physics::ContactFact &fact : facts) {
        PhysicsContact contact;
        contact.phase = fact.phase;
        contact.a = ECS::Entity::FromRaw(fact.ownerA);
        contact.b = ECS::Entity::FromRaw(fact.ownerB);
        contact.point = fact.point;
        contact.normal = fact.normal;
        contacts.Emit(contact);
    }
}

void PhysicsBridge::State::FollowDestroyed() {
    // Every tracked entity has its component while nothing went: the counts say whether to look.
    const std::size_t present = bodiesPresent.Count() + charactersPresent.Count();
    if (present >= tracked.size()) {
        return;
    }
    leaving.clear();
    for (std::size_t index = tracked.size(); index-- > 0;) {
        const Tracked &entry = tracked[index];
        const bool kept = entities.IsAlive(entry.entity) &&
                          (entry.character ? entities.Has<PhysicsCharacter>(entry.entity)
                                           : entities.Has<PhysicsBody>(entry.entity));
        if (!kept) {
            Untrack(index);
            ++followedDestroyed;
        }
    }
    world.RemoveBodies(leaving);
}

PhysicsBridge::PhysicsBridge(SimulationRuntime &runtime, const Physics::PhysicsWorldSpec &spec)
    : m_State(std::make_unique<State>(runtime, spec)) {
    State &state = *m_State;
    ECS::EntityWorld &entities = state.entities;
    ECS::ComponentOptions body;
    body.description = kPhysicsBodyDescription;
    body.format = &FormatBody;
    entities.RegisterComponent<PhysicsBody>(kPhysicsBodyName, body);
    ECS::ComponentOptions character;
    character.description = kPhysicsCharacterDescription;
    character.format = &FormatCharacter;
    entities.RegisterComponent<PhysicsCharacter>(kPhysicsCharacterName, character);

    state.kinematicTargets =
        entities.MakeQuery<const PhysicsBody, const World::WorldTransform>(kKinematicSyncQuery);
    state.bodyWriteBack =
        entities.MakeQuery<const PhysicsBody, World::WorldTransform>(kBodyWriteBackQuery);
    state.characterMoves = entities.MakeQuery<PhysicsCharacter>(kCharacterMoveQuery);
    state.characterWriteBack =
        entities.MakeQuery<const PhysicsCharacter, World::WorldTransform>(kCharacterWriteBackQuery);
    state.bodiesPresent = entities.MakeQuery<const PhysicsBody>(kBodiesPresentQuery);
    state.charactersPresent = entities.MakeQuery<const PhysicsCharacter>(kCharactersPresentQuery);

    runtime.AddSystem({kPhysicsStructureSystemName, ScheduleSlot::Hz30Realtime,
                       [&state](const TickContext &) { state.ApplyRequests(); },
                       RealtimePhase::PhysicsStructure});
    runtime.AddSystem({kPhysicsStepSystemName, ScheduleSlot::Hz30Realtime,
                       [&state](const TickContext &context) { state.SyncBeforeStep(context.dt); },
                       RealtimePhase::PhysicsStep});
    runtime.AddSystem({kPhysicsWriteBackSystemName, ScheduleSlot::Hz30Realtime,
                       [&state](const TickContext &) { state.WriteBack(); },
                       RealtimePhase::PostPhysics});
}

PhysicsBridge::~PhysicsBridge() = default;

void PhysicsBridge::AddBody(ECS::Entity entity, const Physics::BodyDesc &desc) {
    State &state = *m_State;
    RUNIC_ASSERT_MSG(!state.trackedIndex.contains(entity) && !state.IsPending(entity),
                     "the entity already has a body or a character");
    state.pendingBodies.push_back({entity, desc});
}

void PhysicsBridge::AddCharacter(ECS::Entity entity, const Physics::CharacterDesc &desc) {
    State &state = *m_State;
    RUNIC_ASSERT_MSG(!state.trackedIndex.contains(entity) && !state.IsPending(entity),
                     "the entity already has a body or a character");
    state.pendingCharacters.push_back({entity, desc});
}

void PhysicsBridge::Remove(ECS::Entity entity) {
    State &state = *m_State;
    const auto waitingBodies = std::erase_if(
        state.pendingBodies, [entity](const State::PendingBody &p) { return p.entity == entity; });
    const auto waitingCharacters =
        std::erase_if(state.pendingCharacters,
                      [entity](const State::PendingCharacter &p) { return p.entity == entity; });
    if (waitingBodies + waitingCharacters > 0) {
        return; // it never got one
    }
    if (state.trackedIndex.contains(entity)) {
        state.pendingRemovals.push_back(entity);
    }
}

Physics::PhysicsWorld &PhysicsBridge::World() noexcept { return m_State->world; }
const Physics::PhysicsWorld &PhysicsBridge::World() const noexcept { return m_State->world; }
Events::EventBuffer<PhysicsContact> &PhysicsBridge::Contacts() noexcept {
    return m_State->contacts;
}

PhysicsBridgeStats PhysicsBridge::Stats() const noexcept {
    const State &state = *m_State;
    PhysicsBridgeStats stats;
    stats.bodies = state.bodies;
    stats.characters = state.characters;
    stats.pending = static_cast<std::uint32_t>(
        state.pendingBodies.size() + state.pendingCharacters.size() + state.pendingRemovals.size());
    stats.followedDestroyed = state.followedDestroyed;
    return stats;
}

void PhysicsBridge::AtBarrier() { m_State->FollowDestroyed(); }

PhysicsBridge &SimulationRuntime::EnablePhysics(const Physics::PhysicsWorldSpec &spec) {
    State &state = *m_State;
    RUNIC_ASSERT_MSG(!state.stepping, "physics was enabled while a master step runs");
    RUNIC_ASSERT_MSG(state.physics == nullptr, "physics is already enabled");
    state.physics = std::make_unique<PhysicsBridge>(*this, spec);
    return *state.physics;
}

PhysicsBridge *SimulationRuntime::FindPhysicsBridge() noexcept { return m_State->physics.get(); }

} // namespace Runic::Simulation
