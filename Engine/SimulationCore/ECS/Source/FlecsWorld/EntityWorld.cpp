// The EntityWorld (EntityWorld.hpp) on Flecs: the world, its entities, its registered components
// and its cached queries. Flecs stays in this folder; the rest of the Engine sees Runic types.

#include "EntityWorld.hpp"

#include <flecs.h>

#include <string>
#include <unordered_map>
#include <vector>

#include "FlecsWorld/WorldAccess.hpp"
#include "Registration/ComponentNames.hpp"

namespace Runic::ECS {

namespace {

// The name Flecs knows the tag by that marks every entity made through an EntityWorld.
constexpr const char *kEntityTagName = "runic:entity";

// The stage a world queues held-back changes on: the one every world has.
constexpr std::int32_t kMainStage = 0;

// A registered component. The lifecycle sits behind a pointer because Flecs keeps its address for
// as long as the world lives.
struct ComponentRecord {
    std::string name;
    std::string description;
    std::size_t size = 0;
    std::size_t alignment = 0;
    ComponentFormatter format = nullptr;
    ecs_entity_t id = 0;
    std::unique_ptr<Detail::ComponentLifecycle> lifecycle;
};

struct QueryRecord {
    std::string name;
    ecs_query_t *query = nullptr;
    std::uint32_t terms = 0;
    std::array<std::size_t, kMaxQueryTerms> sizes{};
};

// Flecs calls these with the component's type information; the Runic lifecycle the component was
// registered with is its binding context.
const Detail::ComponentLifecycle &LifecycleOf(const ecs_type_info_t *info) {
    return *static_cast<const Detail::ComponentLifecycle *>(info->hooks.binding_ctx);
}

void ConstructHook(void *first, std::int32_t count, const ecs_type_info_t *info) {
    LifecycleOf(info).construct(first, count);
}

void DestroyHook(void *first, std::int32_t count, const ecs_type_info_t *info) {
    LifecycleOf(info).destroy(first, count);
}

void CopyHook(void *to, const void *from, std::int32_t count, const ecs_type_info_t *info) {
    LifecycleOf(info).copy(to, from, count);
}

void MoveHook(void *to, void *from, std::int32_t count, const ecs_type_info_t *info) {
    LifecycleOf(info).move(to, from, count);
}

} // namespace

struct EntityWorld::State {
    // A world with the core only: the Engine schedules its own systems (Runic::Simulation), so
    // the library's pipeline, timers and other add-ons are never imported.
    State() : world(ecs_mini()), stage(ecs_get_stage(world, kMainStage)) {
        ecs_entity_desc_t tag = {};
        tag.name = kEntityTagName;
        entityTag = ecs_entity_init(world, &tag);
    }

    // The world goes first: destroying it destroys the components, which calls the lifecycles
    // the records below hold.
    ~State() { ecs_fini(world); }

    // Flecs 4.2 has no deferred mode: an operation given the world happens at once, and one
    // given a stage is queued until the stage is merged. While changes are held back
    // (deferDepth > 0) the world's operations go to its main stage.
    ecs_world_t *world = nullptr;
    ecs_world_t *stage = nullptr;
    std::uint32_t deferDepth = 0;
    ecs_entity_t entityTag = 0;

    ecs_world_t *Target() const noexcept { return deferDepth > 0 ? stage : world; }

    void BeginDeferred() noexcept { ++deferDepth; }

    void EndDeferred() noexcept {
        RUNIC_ASSERT(deferDepth > 0);
        if (--deferDepth == 0) {
            ecs_merge(stage);
            ++applied;
        }
    }

    // How many times held-back changes have been applied: the state storage uses it to know
    // that what it was holding back is in the world now.
    std::uint64_t applied = 0;

    std::vector<std::uint64_t> componentIds; // by Detail::TypeIndexOf
    std::vector<ComponentRecord> components; // in registration order
    std::unordered_map<ecs_id_t, std::uint32_t> componentOfId;
    std::vector<QueryRecord> queries;

    ComponentInfo InfoOf(const ComponentRecord &record) const {
        ComponentInfo info;
        info.name = record.name;
        info.description = record.description;
        info.size = record.size;
        info.alignment = record.alignment;
        info.format = record.format;
        return info;
    }
};

EntityWorld::EntityWorld() : m_State(std::make_unique<State>()) {}

EntityWorld::~EntityWorld() = default;

// ---- Components --------------------------------------------------------------------------------

void EntityWorld::RegisterRaw(const Detail::ComponentRegistration &registration) {
    State &state = *m_State;
    RUNIC_ASSERT_MSG(Detail::IsNamespacedName(registration.name),
                     "a component's name is 'namespace:name' in lowercase");
    if (registration.typeIndex < state.componentIds.size() &&
        state.componentIds[registration.typeIndex] != kNoComponent) {
        const std::uint32_t index =
            state.componentOfId.at(state.componentIds[registration.typeIndex]);
        RUNIC_ASSERT_MSG(state.components[index].name == registration.name,
                         "a component type was registered again under another name");
        return;
    }
    for (const ComponentRecord &other : state.components) {
        RUNIC_ASSERT_MSG(other.name != registration.name,
                         "two component types were registered under one name");
    }

    ComponentRecord record;
    record.name = registration.name;
    record.description = registration.options.description;
    record.size = registration.size;
    record.alignment = registration.alignment;
    record.format = registration.options.format;
    record.lifecycle = std::make_unique<Detail::ComponentLifecycle>(registration.lifecycle);

    ecs_entity_desc_t entity = {};
    entity.name = record.name.c_str();
    ecs_component_desc_t component = {};
    component.entity = ecs_entity_init(state.world, &entity);
    component.type.size = static_cast<ecs_size_t>(registration.size);
    component.type.alignment = static_cast<ecs_size_t>(registration.alignment);
    const Detail::ComponentLifecycle &lifecycle = *record.lifecycle;
    component.type.hooks.ctor = lifecycle.construct != nullptr ? &ConstructHook : nullptr;
    component.type.hooks.dtor = lifecycle.destroy != nullptr ? &DestroyHook : nullptr;
    component.type.hooks.copy = lifecycle.copy != nullptr ? &CopyHook : nullptr;
    component.type.hooks.move = lifecycle.move != nullptr ? &MoveHook : nullptr;
    component.type.hooks.binding_ctx = record.lifecycle.get();
    record.id = ecs_component_init(state.world, &component);
    RUNIC_ASSERT_MSG(record.id != 0, "the component could not be registered");

    if (state.componentIds.size() <= registration.typeIndex) {
        state.componentIds.resize(registration.typeIndex + 1, kNoComponent);
    }
    state.componentIds[registration.typeIndex] = record.id;
    state.componentOfId.emplace(record.id, static_cast<std::uint32_t>(state.components.size()));
    state.components.push_back(std::move(record));
    m_ComponentIds = state.componentIds.data();
    m_ComponentIdCount = static_cast<std::uint32_t>(state.componentIds.size());
}

std::uint32_t EntityWorld::ComponentCount() const noexcept {
    return static_cast<std::uint32_t>(m_State->components.size());
}

ComponentInfo EntityWorld::DescribeComponent(std::uint32_t index) const {
    RUNIC_ASSERT(index < m_State->components.size());
    return m_State->InfoOf(m_State->components[index]);
}

// ---- Entities ----------------------------------------------------------------------------------

Entity EntityWorld::Create() {
    return Entity::FromRaw(ecs_new_w_id(m_State->Target(), m_State->entityTag));
}

void EntityWorld::Destroy(Entity entity) {
    if (IsAlive(entity)) {
        ecs_delete(m_State->Target(), entity.Raw());
    }
}

bool EntityWorld::IsAlive(Entity entity) const noexcept {
    return !entity.IsNull() && ecs_is_alive(m_State->world, entity.Raw());
}

std::uint32_t EntityWorld::EntityCount() const {
    return static_cast<std::uint32_t>(ecs_count_id(m_State->world, m_State->entityTag));
}

void EntityWorld::SetRaw(Entity entity, std::uint64_t component, std::size_t size,
                         const void *value) {
    RUNIC_ASSERT_MSG(IsAlive(entity), "a component was set on an entity that is not alive");
    ecs_set_id(m_State->Target(), entity.Raw(), component, size, value);
}

void *EntityWorld::GetMutableRaw(Entity entity, std::uint64_t component) noexcept {
    RUNIC_ASSERT_MSG(IsAlive(entity), "a component was read from an entity that is not alive");
    return ecs_get_mut_id(m_State->world, entity.Raw(), component);
}

const void *EntityWorld::GetRaw(Entity entity, std::uint64_t component) const noexcept {
    RUNIC_ASSERT_MSG(IsAlive(entity), "a component was read from an entity that is not alive");
    return ecs_get_id(m_State->world, entity.Raw(), component);
}

bool EntityWorld::HasRaw(Entity entity, std::uint64_t component) const noexcept {
    RUNIC_ASSERT_MSG(IsAlive(entity), "a component was asked of an entity that is not alive");
    return ecs_has_id(m_State->world, entity.Raw(), component);
}

void EntityWorld::RemoveRaw(Entity entity, std::uint64_t component) {
    RUNIC_ASSERT_MSG(IsAlive(entity), "a component was removed from an entity that is not alive");
    ecs_remove_id(m_State->Target(), entity.Raw(), component);
}

// ---- Queries -----------------------------------------------------------------------------------

std::uint32_t EntityWorld::MakeQueryRaw(std::string_view name,
                                        std::span<const Detail::QueryTerm> terms) {
    State &state = *m_State;
    RUNIC_ASSERT(!name.empty());
    RUNIC_ASSERT(!terms.empty() && terms.size() <= kMaxQueryTerms);

    QueryRecord record;
    record.name = name;
    record.terms = static_cast<std::uint32_t>(terms.size());
    ecs_query_desc_t description = {};
    for (std::size_t term = 0; term < terms.size(); ++term) {
        const std::uint32_t typeIndex = terms[term].typeIndex;
        RUNIC_ASSERT_MSG(typeIndex < state.componentIds.size() &&
                             state.componentIds[typeIndex] != kNoComponent,
                         "a query names a component type that is not registered in this world");
        const ecs_id_t id = state.componentIds[typeIndex];
        description.terms[term].id = id;
        description.terms[term].inout = terms[term].readOnly ? EcsIn : EcsInOut;
        record.sizes[term] = state.components[state.componentOfId.at(id)].size;
    }
    description.cache_kind = EcsQueryCacheAuto;
    record.query = ecs_query_init(state.world, &description);
    RUNIC_ASSERT_MSG(record.query != nullptr, "the query could not be made");
    state.queries.push_back(std::move(record));
    return static_cast<std::uint32_t>(state.queries.size() - 1);
}

void EntityWorld::IterateQuery(std::uint32_t query, Detail::ChunkVisitor visitor, void *user) {
    State &state = *m_State;
    RUNIC_ASSERT(query < state.queries.size());
    const QueryRecord &record = state.queries[query];

    // Entities do not move while the query's storage is walked: changes wait for the end.
    state.BeginDeferred();
    ecs_iter_t it = ecs_query_iter(state.world, record.query);
    while (ecs_query_next(&it)) {
        Detail::QueryChunk chunk;
        chunk.count = static_cast<std::uint32_t>(it.count);
        chunk.entities = reinterpret_cast<const Entity *>(it.entities);
        for (std::uint32_t term = 0; term < record.terms; ++term) {
            chunk.columns[term] =
                ecs_field_w_size(&it, record.sizes[term], static_cast<std::int8_t>(term));
        }
        visitor(user, chunk);
    }
    state.EndDeferred();
}

std::uint32_t EntityWorld::CountQuery(std::uint32_t query) const {
    RUNIC_ASSERT(query < m_State->queries.size());
    return static_cast<std::uint32_t>(ecs_query_count(m_State->queries[query].query).entities);
}

std::uint32_t EntityWorld::QueryCount() const noexcept {
    return static_cast<std::uint32_t>(m_State->queries.size());
}

QueryInfo EntityWorld::DescribeQuery(std::uint32_t index) const {
    RUNIC_ASSERT(index < m_State->queries.size());
    QueryInfo info;
    info.name = m_State->queries[index].name;
    info.terms = m_State->queries[index].terms;
    return info;
}

// ---- Holding changes back ----------------------------------------------------------------------

void EntityWorld::BeginDeferred() { m_State->BeginDeferred(); }

void EntityWorld::EndDeferred() { m_State->EndDeferred(); }

bool EntityWorld::IsDeferred() const noexcept { return m_State->deferDepth > 0; }

// ---- For tools ---------------------------------------------------------------------------------

void EntityWorld::ForEachEntity(const std::function<void(Entity)> &visit) const {
    m_State->BeginDeferred();
    ecs_iter_t it = ecs_each_id(m_State->world, m_State->entityTag);
    while (ecs_each_next(&it)) {
        for (std::int32_t row = 0; row < it.count; ++row) {
            visit(Entity::FromRaw(it.entities[row]));
        }
    }
    m_State->EndDeferred();
}

void EntityWorld::ForEachComponentOf(
    Entity entity,
    const std::function<void(const ComponentInfo &, const void *component)> &visit) const {
    RUNIC_ASSERT_MSG(IsAlive(entity), "the components of an entity that is not alive were asked");
    const State &state = *m_State;
    const ecs_type_t *type = ecs_get_type(state.world, entity.Raw());
    if (type == nullptr) {
        return;
    }
    for (std::int32_t index = 0; index < type->count; ++index) {
        const auto found = state.componentOfId.find(type->array[index]);
        if (found == state.componentOfId.end()) {
            continue; // the entity tag, a state's storage: not a registered component
        }
        const ComponentRecord &record = state.components[found->second];
        visit(state.InfoOf(record), ecs_get_id(state.world, entity.Raw(), record.id));
    }
}

ArchetypeId EntityWorld::ArchetypeOf(Entity entity) const noexcept {
    RUNIC_ASSERT_MSG(IsAlive(entity), "the archetype of an entity that is not alive was asked");
    return ArchetypeId{
        reinterpret_cast<std::uint64_t>(ecs_get_table(m_State->world, entity.Raw()))};
}

void *EntityWorld::NativeWorldHandle() const noexcept { return m_State->world; }

namespace Detail {

ecs_world_t *WorldAccess::World(const EntityWorld &world) noexcept { return world.m_State->world; }

ecs_world_t *WorldAccess::Target(const EntityWorld &world) noexcept {
    return world.m_State->Target();
}

std::uint64_t WorldAccess::AppliedCount(const EntityWorld &world) noexcept {
    return world.m_State->applied;
}

} // namespace Detail

} // namespace Runic::ECS
