#pragma once

#include <cstdint>
#include <memory>
#include <string_view>

#include "Entity.hpp"
#include "EntityWorld.hpp"

// State declarations: one declaration per state of an entity, and the declaration's category
// picks how the state is stored (RunicDocs › Simulation core › Flecs › State declarations):
//
//   HotFlag      Stunned, Mounted, InCombat     a toggled marker: flipping it is a bit, and the
//                                               entity stays where it is stored
//   Relation     MountedOn(horse)               a fact with a target; with many distinct targets
//                                               the pair is kept beside the entity's storage
//   ContentFlag  MetTheHarbormaster             one bit in the entity's compact state container
//   Value        a reputation, a counter        one number in the same container
//   Derived      IsNight, IsHealthLow           not stored: a condition computed when asked
//
// One declaration, many readers: abilities, AI, dialogue, quests, flows and the client UI ask for
// a state by its ID or its name, and never care how it is stored. Each state has one owner, the
// one system that writes it. An observable state reports entering and leaving as facts, which is
// what lets it be a trigger.

namespace Runic::ECS {

enum class StateCategory : std::uint8_t { HotFlag, Relation, ContentFlag, Value, Derived };

// Who may ever be told about a state; replication reads it (Network, later).
enum class StateVisibility : std::uint8_t { ServerOnly, Owner, Everyone };

// How long a state lasts; handoff and persistence read it (later).
enum class StateLifetime : std::uint8_t {
    Transient, // gone when its simulation ends
    Handoff,   // travels with the entity to another simulation
    Durable,   // saved with the entity
};

// How many distinct targets a Relation is expected to have. With many (a mount, a tether, an
// owner) every pair would otherwise need storage of its own, so it is kept beside the entity's
// storage and setting it never moves the entity. With few (a team, a faction) entities with the
// same target are stored together, and setting it moves the entity like adding a component.
enum class TargetCardinality : std::uint8_t { Few, Many };

// A Derived state's condition. It reads the world and stores nothing.
using DerivedCondition = bool (*)(const EntityWorld &world, Entity entity, void *user);

struct StateDeclaration {
    std::string_view name;        // "namespace:name", unique among the world's states
    std::string_view description; // one line for tools
    StateCategory category = StateCategory::HotFlag;
    std::string_view owner;  // the one system that writes it; a Derived state is owned by its rule
    bool observable = false; // entering and leaving it are facts; not for Derived
    StateVisibility visibility = StateVisibility::ServerOnly;
    StateLifetime lifetime = StateLifetime::Transient;
    TargetCardinality targets = TargetCardinality::Many; // Relation only
    DerivedCondition condition = nullptr;                // Derived only, and required there
    void *conditionUser = nullptr;
};

// The handle of a declared state.
class StateId {
  public:
    constexpr StateId() noexcept = default;
    constexpr bool IsValid() const noexcept { return m_Index != kInvalidIndex; }
    constexpr std::uint32_t Index() const noexcept { return m_Index; }

    friend constexpr bool operator==(StateId, StateId) noexcept = default;

  private:
    friend class EntityStates;

    static constexpr std::uint32_t kInvalidIndex = 0xFFFF'FFFF;

    constexpr explicit StateId(std::uint32_t index) noexcept : m_Index(index) {}

    std::uint32_t m_Index = kInvalidIndex;
};

// A declared state as tools see it. The texts stay valid while the EntityStates lives.
struct StateInfo {
    std::string_view name;
    std::string_view description;
    StateCategory category = StateCategory::HotFlag;
    std::string_view owner;
    bool observable = false;
    StateVisibility visibility = StateVisibility::ServerOnly;
    StateLifetime lifetime = StateLifetime::Transient;
    TargetCardinality targets = TargetCardinality::Many;
};

// The facts of an observable state. target is the Relation's target, null for the other
// categories. A Relation that changes its target exits the old one, then enters the new one.
struct StateEntered {
    Entity entity;
    StateId state;
    Entity target;

    friend constexpr bool operator==(const StateEntered &, const StateEntered &) noexcept = default;
};

struct StateExited {
    Entity entity;
    StateId state;
    Entity target;

    friend constexpr bool operator==(const StateExited &, const StateExited &) noexcept = default;
};

// Where an EntityStates reports the facts of observable states, at the moment the owner asks for
// the change. The simulation implements it and emits the facts into its event buffers.
class StateFactSink {
  public:
    virtual void OnStateEntered(const StateEntered &fact) = 0;
    virtual void OnStateExited(const StateExited &fact) = 0;

  protected:
    ~StateFactSink() = default;
};

// The states of the entities of one world.
class EntityStates {
  public:
    // The world outlives the EntityStates.
    explicit EntityStates(EntityWorld &world);
    ~EntityStates();

    EntityStates(const EntityStates &) = delete;
    EntityStates &operator=(const EntityStates &) = delete;

    // ---- Declarations --------------------------------------------------------------------------

    // Declares a state, at setup. Hot flags are declared before the entities that use them are
    // attached; content flags and values may be declared at any time.
    StateId Declare(const StateDeclaration &declaration);

    // The state of that name, or an invalid ID.
    StateId Find(std::string_view name) const noexcept;
    std::uint32_t Count() const noexcept;
    StateInfo Describe(StateId state) const;

    // Where observable states report; nullptr reports nowhere. The sink outlives its use.
    void SetFactSink(StateFactSink *sink) noexcept;

    // ---- An entity's storage -------------------------------------------------------------------

    // Gives an entity the storage of every hot flag declared so far and its state container, in
    // one go, so that flipping its states later never moves it. Whoever spawns an entity
    // attaches it; an entity that was not attached gets the storage a state needs the first time
    // that state is written, which moves it once.
    void Attach(Entity entity);

    // ---- Reading -------------------------------------------------------------------------------

    // True when the entity is in the state: the flag is set, the relation has a target, the
    // value was given, or the Derived condition holds.
    bool Has(Entity entity, StateId state) const;
    // A Relation's target, or the null entity.
    Entity TargetOf(Entity entity, StateId state) const;
    // A Value, or 0 when the entity was given none.
    std::int64_t ValueOf(Entity entity, StateId state) const;

    // ---- Writing: the owner's calls ------------------------------------------------------------
    //
    // A change that does not move the entity applies at once, also inside a query's iteration:
    // a hot flag, a content flag, a value, a Relation with many targets, on an attached entity.
    // A change that moves the entity (a Relation with few targets; the first write on an entity
    // that was not attached) follows the world's rule: inside an iteration or a DeferScope it
    // applies when that ends, and until then reads show the state as it was. The fact of an
    // observable state is reported when the change is asked for, once.

    // Sets a HotFlag or a ContentFlag.
    void Set(Entity entity, StateId state);
    // Gives a Relation its target, replacing the one it had.
    void SetTarget(Entity entity, StateId state, Entity target);
    // Gives a Value.
    void SetValue(Entity entity, StateId state, std::int64_t value);
    // Takes the entity out of the state: clears the flag, the target or the value.
    void Clear(Entity entity, StateId state);

  private:
    struct State;
    std::unique_ptr<State> m_State;
};

} // namespace Runic::ECS
