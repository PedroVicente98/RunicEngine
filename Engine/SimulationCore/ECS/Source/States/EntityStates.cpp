// State declarations (EntityStates.hpp) on Flecs: each category's storage, and the facts.
//
//   HotFlag      a tag with the CanToggle trait; set = enabled. Flipping it writes one bit.
//   Relation     an exclusive relationship; with many targets it also has the DontFragment trait.
//   ContentFlag  a bit, and
//   Value        a number, in the entity's StateContainer component.
//   Derived      nothing stored.

#include "EntityStates.hpp"

#include <flecs.h>

#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "FlecsWorld/WorldAccess.hpp"
#include "Registration/ComponentNames.hpp"
#include "States/StateContainer.hpp"

namespace Runic::ECS {

namespace {

using Detail::StateContainer;
using Detail::WorldAccess;

constexpr std::string_view kContainerName = "engine:state_container";
constexpr std::string_view kContainerDescription =
    "The content flags and values of an entity (EntityStates)";
// Flecs names of the entities that stand for states, apart from the components' names.
constexpr std::string_view kStateNamePrefix = "runic:state:";
constexpr std::int32_t kFirstTarget = 0; // an exclusive relationship has one target, the first

struct Declared {
    std::string name;
    std::string description;
    std::string owner;
    StateCategory category = StateCategory::HotFlag;
    bool observable = false;
    StateVisibility visibility = StateVisibility::ServerOnly;
    StateLifetime lifetime = StateLifetime::Transient;
    TargetCardinality targets = TargetCardinality::Many;
    DerivedCondition condition = nullptr;
    void *conditionUser = nullptr;

    ecs_entity_t id = 0;    // HotFlag: the toggled tag; Relation: the relationship
    std::uint32_t slot = 0; // ContentFlag and Value: the place in the container
};

// What an entity's held-back changes will make of its states: the changes that had to wait for
// the end of an iteration or a DeferScope because they move the entity.
struct HeldBack {
    std::vector<std::pair<std::uint32_t, bool>> hotFlags;  // by state index
    std::vector<std::pair<std::uint32_t, Entity>> targets; // by state index
    std::optional<StateContainer> container;

    template <class Value>
    static Value *Find(std::vector<std::pair<std::uint32_t, Value>> &entries, std::uint32_t state) {
        for (auto &[index, value] : entries) {
            if (index == state) {
                return &value;
            }
        }
        return nullptr;
    }
};

void FormatContainer(const void *component, std::string &out) {
    const auto &container = *static_cast<const StateContainer *>(component);
    std::size_t flags = 0;
    for (const std::uint64_t word : container.flags) {
        flags += static_cast<std::size_t>(__builtin_popcountll(word));
    }
    std::size_t values = 0;
    for (const std::uint64_t word : container.given) {
        values += static_cast<std::size_t>(__builtin_popcountll(word));
    }
    out = std::to_string(flags) + " content flag(s), " + std::to_string(values) + " value(s)";
}

} // namespace

struct EntityStates::State {
    explicit State(EntityWorld &entityWorld)
        : world(entityWorld), native(WorldAccess::World(entityWorld)) {}

    EntityWorld &world;
    ecs_world_t *native;
    std::vector<Declared> states;
    std::unordered_map<std::string, std::uint32_t> byName;
    std::vector<ecs_entity_t> hotFlags;
    std::uint32_t contentFlags = 0;
    std::uint32_t values = 0;
    StateFactSink *sink = nullptr;

    std::unordered_map<std::uint64_t, HeldBack> heldBack;
    std::uint64_t heldBackAt = 0; // the world's applied count when heldBack was started

    const Declared &Of(StateId state) const {
        RUNIC_ASSERT_MSG(state.Index() < states.size(), "the state was not declared here");
        return states[state.Index()];
    }

    // What was held back is in the world once the world has applied its changes.
    void ForgetApplied() {
        if (!heldBack.empty() && WorldAccess::AppliedCount(world) != heldBackAt) {
            heldBack.clear();
        }
    }

    HeldBack &HeldBackOf(Entity entity) {
        if (heldBack.empty()) {
            heldBackAt = WorldAccess::AppliedCount(world);
        }
        return heldBack[entity.Raw()];
    }

    HeldBack *FindHeldBack(Entity entity) {
        const auto found = heldBack.find(entity.Raw());
        return found != heldBack.end() ? &found->second : nullptr;
    }

    void Entered(const Declared &declared, Entity entity, StateId state, Entity target) const {
        if (declared.observable && sink != nullptr) {
            sink->OnStateEntered(StateEntered{entity, state, target});
        }
    }

    void Exited(const Declared &declared, Entity entity, StateId state, Entity target) const {
        if (declared.observable && sink != nullptr) {
            sink->OnStateExited(StateExited{entity, state, target});
        }
    }

    // ---- Hot flags -----------------------------------------------------------------------------

    void WriteHotFlag(const Declared &declared, Entity entity, StateId state, bool set) {
        const ecs_entity_t raw = entity.Raw();
        if (ecs_has_id(native, raw, declared.id)) {
            // The storage is there: one bit, at once.
            if (ecs_is_enabled_id(native, raw, declared.id) != set) {
                ecs_enable_id(native, raw, declared.id, set);
                Report(declared, entity, state, Entity(), set);
            }
            return;
        }
        if (!world.IsDeferred()) {
            if (set) {
                ecs_add_id(native, raw, declared.id); // moves the entity, this once
                ecs_enable_id(native, raw, declared.id, true);
                Report(declared, entity, state, Entity(), true);
            }
            return;
        }
        // The entity has no storage for the flag and may not move now: the change is held back.
        HeldBack &held = HeldBackOf(entity);
        bool *pending = HeldBack::Find(held.hotFlags, state.Index());
        const bool was = pending != nullptr && *pending;
        if (was == set) {
            return;
        }
        if (pending != nullptr) {
            *pending = set;
        } else {
            held.hotFlags.emplace_back(state.Index(), set);
        }
        ecs_world_t *stage = WorldAccess::Target(world);
        ecs_add_id(stage, raw, declared.id);
        ecs_enable_id(stage, raw, declared.id, set);
        Report(declared, entity, state, Entity(), set);
    }

    // ---- The container: content flags and values -----------------------------------------------

    // Runs change(container) on the entity's container, wherever it is now: in the world, or
    // held back with the entity's other changes. change returns whether it changed anything.
    template <class Change> bool ChangeContainer(Entity entity, bool creates, Change &&change) {
        if (StateContainer *container = world.TryGet<StateContainer>(entity)) {
            return change(*container); // in place, at once
        }
        if (!world.IsDeferred()) {
            if (!creates) {
                return false; // nothing to clear
            }
            StateContainer fresh;
            const bool changed = change(fresh);
            world.Set(entity, fresh); // moves the entity, this once
            return changed;
        }
        HeldBack *held = FindHeldBack(entity);
        if ((held == nullptr || !held->container.has_value()) && !creates) {
            return false;
        }
        std::optional<StateContainer> &pending = HeldBackOf(entity).container;
        if (!pending.has_value()) {
            pending.emplace();
        }
        const bool changed = change(*pending);
        if (changed) {
            world.Set(entity, *pending); // held back; the last one carries every change
        }
        return changed;
    }

    // ---- Relations -----------------------------------------------------------------------------

    Entity StoredTarget(const Declared &declared, Entity entity) const {
        return Entity::FromRaw(ecs_get_target(native, entity.Raw(), declared.id, kFirstTarget));
    }

    void WriteTarget(const Declared &declared, Entity entity, StateId state, Entity target) {
        const ecs_entity_t raw = entity.Raw();
        // A relation with many targets never moves the entity: at once. One with few targets
        // moves it, so it waits with the world's other held-back changes.
        const bool holdsBack = declared.targets == TargetCardinality::Few && world.IsDeferred();
        Entity *pending = nullptr;
        if (holdsBack) {
            HeldBack &held = HeldBackOf(entity);
            pending = HeldBack::Find(held.targets, state.Index());
            if (pending == nullptr) {
                held.targets.emplace_back(state.Index(), StoredTarget(declared, entity));
                pending = &held.targets.back().second;
            }
        }
        const Entity was = pending != nullptr ? *pending : StoredTarget(declared, entity);
        if (was == target) {
            return;
        }
        ecs_world_t *where = holdsBack ? WorldAccess::Target(world) : native;
        if (!was.IsNull()) {
            Exited(declared, entity, state, was);
        }
        if (target.IsNull()) {
            ecs_remove_id(where, raw, ecs_pair(declared.id, EcsWildcard));
        } else {
            ecs_add_id(where, raw, ecs_pair(declared.id, target.Raw())); // exclusive: replaces
            Entered(declared, entity, state, target);
        }
        if (pending != nullptr) {
            *pending = target;
        }
    }

    void Report(const Declared &declared, Entity entity, StateId state, Entity target,
                bool entered) const {
        if (entered) {
            Entered(declared, entity, state, target);
        } else {
            Exited(declared, entity, state, target);
        }
    }
};

EntityStates::EntityStates(EntityWorld &world) : m_State(std::make_unique<State>(world)) {
    ComponentOptions options;
    options.description = kContainerDescription;
    options.format = &FormatContainer;
    world.RegisterComponent<StateContainer>(kContainerName, options);
}

EntityStates::~EntityStates() = default;

// ---- Declarations ------------------------------------------------------------------------------

StateId EntityStates::Declare(const StateDeclaration &declaration) {
    State &state = *m_State;
    RUNIC_ASSERT_MSG(Detail::IsNamespacedName(declaration.name),
                     "a state's name is 'namespace:name' in lowercase");
    RUNIC_ASSERT_MSG(!state.byName.contains(std::string(declaration.name)),
                     "a state was declared twice");
    const bool derived = declaration.category == StateCategory::Derived;
    RUNIC_ASSERT_MSG(derived == (declaration.condition != nullptr),
                     "a Derived state has a condition, and only a Derived state");
    RUNIC_ASSERT_MSG(derived || !declaration.owner.empty(),
                     "a stored state names its owner, the one system that writes it");
    RUNIC_ASSERT_MSG(!(derived && declaration.observable),
                     "a Derived state is computed when asked: it cannot report facts");

    Declared declared;
    declared.name = declaration.name;
    declared.description = declaration.description;
    declared.owner = declaration.owner;
    declared.category = declaration.category;
    declared.observable = declaration.observable;
    declared.visibility = declaration.visibility;
    declared.lifetime = declaration.lifetime;
    declared.targets = declaration.targets;
    declared.condition = declaration.condition;
    declared.conditionUser = declaration.conditionUser;

    const bool stored = declaration.category == StateCategory::HotFlag ||
                        declaration.category == StateCategory::Relation;
    if (stored) {
        RUNIC_ASSERT_MSG(!state.world.IsDeferred(),
                         "a hot flag or a relation is declared at setup, not inside a phase");
        const std::string nativeName = std::string(kStateNamePrefix) + declared.name;
        ecs_entity_desc_t entity = {};
        entity.name = nativeName.c_str();
        declared.id = ecs_entity_init(state.native, &entity);
    }
    switch (declaration.category) {
    case StateCategory::HotFlag:
        ecs_add_id(state.native, declared.id, EcsCanToggle);
        state.hotFlags.push_back(declared.id);
        break;
    case StateCategory::Relation:
        ecs_add_id(state.native, declared.id, EcsExclusive);
        if (declaration.targets == TargetCardinality::Many) {
            ecs_add_id(state.native, declared.id, EcsDontFragment);
        }
        break;
    case StateCategory::ContentFlag:
        declared.slot = state.contentFlags++;
        break;
    case StateCategory::Value:
        declared.slot = state.values++;
        break;
    case StateCategory::Derived:
        break;
    }

    const auto index = static_cast<std::uint32_t>(state.states.size());
    state.byName.emplace(declared.name, index);
    state.states.push_back(std::move(declared));
    return StateId(index);
}

StateId EntityStates::Find(std::string_view name) const noexcept {
    const auto found = m_State->byName.find(std::string(name));
    return found != m_State->byName.end() ? StateId(found->second) : StateId();
}

std::uint32_t EntityStates::Count() const noexcept {
    return static_cast<std::uint32_t>(m_State->states.size());
}

StateInfo EntityStates::Describe(StateId state) const {
    const Declared &declared = m_State->Of(state);
    StateInfo info;
    info.name = declared.name;
    info.description = declared.description;
    info.category = declared.category;
    info.owner = declared.owner;
    info.observable = declared.observable;
    info.visibility = declared.visibility;
    info.lifetime = declared.lifetime;
    info.targets = declared.targets;
    return info;
}

void EntityStates::SetFactSink(StateFactSink *sink) noexcept { m_State->sink = sink; }

// ---- An entity's storage -----------------------------------------------------------------------

void EntityStates::Attach(Entity entity) {
    State &state = *m_State;
    RUNIC_ASSERT_MSG(state.world.IsAlive(entity), "an entity that is not alive was attached");
    state.ForgetApplied();
    // Through the world's held-back changes even when nothing else is held back, so the entity
    // moves once for all the storage instead of once per flag.
    const EntityWorld::DeferScope scope(state.world);
    ecs_world_t *stage = WorldAccess::Target(state.world);
    const ecs_entity_t raw = entity.Raw();
    HeldBack *held = state.FindHeldBack(entity);
    for (std::uint32_t index = 0; index < state.states.size(); ++index) {
        const Declared &declared = state.states[index];
        if (declared.category != StateCategory::HotFlag ||
            ecs_has_id(state.native, raw, declared.id)) {
            continue;
        }
        if (held != nullptr && HeldBack::Find(held->hotFlags, index) != nullptr) {
            continue; // a held-back change already brings the storage, with its value
        }
        ecs_add_id(stage, raw, declared.id);
        ecs_enable_id(stage, raw, declared.id, false);
    }
    const bool heldContainer = held != nullptr && held->container.has_value();
    if (!heldContainer && state.world.TryGet<StateContainer>(entity) == nullptr) {
        state.world.Set(entity, StateContainer{});
    }
}

// ---- Reading -----------------------------------------------------------------------------------

bool EntityStates::Has(Entity entity, StateId state) const {
    const State &states = *m_State;
    const Declared &declared = states.Of(state);
    RUNIC_ASSERT_MSG(states.world.IsAlive(entity),
                     "a state was asked of an entity that is not alive");
    switch (declared.category) {
    case StateCategory::HotFlag:
        return ecs_is_enabled_id(states.native, entity.Raw(), declared.id);
    case StateCategory::Relation:
        return !states.StoredTarget(declared, entity).IsNull();
    case StateCategory::ContentFlag: {
        const StateContainer *container =
            std::as_const(states.world).TryGet<StateContainer>(entity);
        return container != nullptr && container->Flag(declared.slot);
    }
    case StateCategory::Value: {
        const StateContainer *container =
            std::as_const(states.world).TryGet<StateContainer>(entity);
        return container != nullptr && container->HasValue(declared.slot);
    }
    case StateCategory::Derived:
        return declared.condition(states.world, entity, declared.conditionUser);
    }
    return false;
}

Entity EntityStates::TargetOf(Entity entity, StateId state) const {
    const Declared &declared = m_State->Of(state);
    RUNIC_ASSERT_MSG(declared.category == StateCategory::Relation, "only a Relation has a target");
    RUNIC_ASSERT_MSG(m_State->world.IsAlive(entity),
                     "a state was asked of an entity that is not alive");
    return m_State->StoredTarget(declared, entity);
}

std::int64_t EntityStates::ValueOf(Entity entity, StateId state) const {
    const Declared &declared = m_State->Of(state);
    RUNIC_ASSERT_MSG(declared.category == StateCategory::Value, "only a Value has a value");
    RUNIC_ASSERT_MSG(m_State->world.IsAlive(entity),
                     "a state was asked of an entity that is not alive");
    const StateContainer *container = std::as_const(m_State->world).TryGet<StateContainer>(entity);
    return container != nullptr ? container->Value(declared.slot) : 0;
}

// ---- Writing -----------------------------------------------------------------------------------

void EntityStates::Set(Entity entity, StateId state) {
    State &states = *m_State;
    const Declared &declared = states.Of(state);
    RUNIC_ASSERT_MSG(declared.category == StateCategory::HotFlag ||
                         declared.category == StateCategory::ContentFlag,
                     "Set is for a HotFlag or a ContentFlag");
    RUNIC_ASSERT_MSG(states.world.IsAlive(entity),
                     "a state was set on an entity that is not alive");
    states.ForgetApplied();
    if (declared.category == StateCategory::HotFlag) {
        states.WriteHotFlag(declared, entity, state, true);
        return;
    }
    const bool changed = states.ChangeContainer(entity, true, [&](StateContainer &container) {
        if (container.Flag(declared.slot)) {
            return false;
        }
        container.SetFlag(declared.slot, true);
        return true;
    });
    if (changed) {
        states.Entered(declared, entity, state, Entity());
    }
}

void EntityStates::SetTarget(Entity entity, StateId state, Entity target) {
    State &states = *m_State;
    const Declared &declared = states.Of(state);
    RUNIC_ASSERT_MSG(declared.category == StateCategory::Relation, "only a Relation has a target");
    RUNIC_ASSERT_MSG(states.world.IsAlive(entity),
                     "a state was set on an entity that is not alive");
    RUNIC_ASSERT_MSG(states.world.IsAlive(target), "a Relation's target must be alive");
    states.ForgetApplied();
    states.WriteTarget(declared, entity, state, target);
}

void EntityStates::SetValue(Entity entity, StateId state, std::int64_t value) {
    State &states = *m_State;
    const Declared &declared = states.Of(state);
    RUNIC_ASSERT_MSG(declared.category == StateCategory::Value, "only a Value has a value");
    RUNIC_ASSERT_MSG(states.world.IsAlive(entity),
                     "a state was set on an entity that is not alive");
    states.ForgetApplied();
    bool entered = false;
    states.ChangeContainer(entity, true, [&](StateContainer &container) {
        entered = !container.HasValue(declared.slot);
        if (!entered && container.Value(declared.slot) == value) {
            return false;
        }
        container.SetValue(declared.slot, value);
        return true;
    });
    if (entered) {
        states.Entered(declared, entity, state, Entity());
    }
}

void EntityStates::Clear(Entity entity, StateId state) {
    State &states = *m_State;
    const Declared &declared = states.Of(state);
    RUNIC_ASSERT_MSG(declared.category != StateCategory::Derived,
                     "a Derived state is computed: it cannot be cleared");
    RUNIC_ASSERT_MSG(states.world.IsAlive(entity),
                     "a state was cleared on an entity that is not alive");
    states.ForgetApplied();
    switch (declared.category) {
    case StateCategory::HotFlag:
        states.WriteHotFlag(declared, entity, state, false);
        break;
    case StateCategory::Relation:
        states.WriteTarget(declared, entity, state, Entity());
        break;
    case StateCategory::ContentFlag: {
        const bool changed = states.ChangeContainer(entity, false, [&](StateContainer &container) {
            if (!container.Flag(declared.slot)) {
                return false;
            }
            container.SetFlag(declared.slot, false);
            return true;
        });
        if (changed) {
            states.Exited(declared, entity, state, Entity());
        }
        break;
    }
    case StateCategory::Value: {
        const bool changed = states.ChangeContainer(entity, false, [&](StateContainer &container) {
            if (!container.HasValue(declared.slot)) {
                return false;
            }
            container.ClearValue(declared.slot);
            return true;
        });
        if (changed) {
            states.Exited(declared, entity, state, Entity());
        }
        break;
    }
    case StateCategory::Derived:
        break;
    }
}

} // namespace Runic::ECS
