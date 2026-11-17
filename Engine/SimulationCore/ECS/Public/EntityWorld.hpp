#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <span>
#include <string_view>
#include <type_traits>
#include <utility>

#include "Assert.hpp"
#include "ComponentInfo.hpp"
#include "Entity.hpp"
#include "EntityQuery.hpp"

// The EntityWorld: the entities of one simulation and their components. It is the Runic face of
// the ECS library the Engine uses; no type of that library appears here, and the only other way
// to the library is the opt-in Runic::ECSNative target, which is discouraged (RunicDocs ›
// Simulation core › Flecs).
//
// The conventions every module follows:
//   - A component is plain data, registered once per world under a namespaced name
//     ("engine:test_position", "game:..."). Hot state is dense and owned by its entity.
//   - Recurring work uses a query made once at setup, never a search per tick.
//   - Only the simulation's own phases change a world. One thread uses a world at a time.
//   - An Entity is local to the process; across processes use Core::GlobalEntityId.

namespace Runic::ECS {

// Which set of components an entity has, as an opaque value: two entities with the same value are
// stored together. It changes when a component is added or removed, and tests and tools use it
// to see that an operation did not move an entity.
struct ArchetypeId {
    std::uint64_t value = 0;

    friend constexpr bool operator==(ArchetypeId, ArchetypeId) noexcept = default;
};

class EntityWorld {
  public:
    EntityWorld();
    ~EntityWorld();

    EntityWorld(const EntityWorld &) = delete;
    EntityWorld &operator=(const EntityWorld &) = delete;

    // ---- Components ----------------------------------------------------------------------------

    // Registers a component type under its name, before its first use in this world. A name is
    // "namespace:name" in lowercase letters, digits and underscores. Registering the same type
    // again with the same name does nothing. T is default-constructible and assignable.
    template <class T>
    void RegisterComponent(std::string_view name, const ComponentOptions &options = {});

    template <class T> bool IsRegistered() const noexcept {
        const std::uint32_t index = Detail::TypeIndexOf<std::remove_cv_t<T>>();
        return index < m_ComponentIdCount && m_ComponentIds[index] != kNoComponent;
    }

    // Every registered component, in registration order, for tools.
    std::uint32_t ComponentCount() const noexcept;
    ComponentInfo DescribeComponent(std::uint32_t index) const;

    // ---- Entities ------------------------------------------------------------------------------

    Entity Create();
    // Destroys an entity and its components. A handle that is null or no longer alive is ignored.
    void Destroy(Entity entity);
    // False for the null entity and for a handle whose entity was destroyed, even if its slot has
    // been reused since.
    bool IsAlive(Entity entity) const noexcept;
    // Entities created through this world and still alive.
    std::uint32_t EntityCount() const;

    // Gives an entity a component value, adding the component if it lacks it. While changes are
    // held back (inside a query's iteration or a DeferScope) the value arrives when they apply;
    // to change a value at once, write through Get or the reference a query gives.
    template <class T> void Set(Entity entity, const T &value) {
        SetRaw(entity, ComponentIdOf<T>(), sizeof(T), std::addressof(value));
    }

    // The entity's value, or nullptr if it lacks the component. The pointer is valid until the
    // next change that moves entities (a component added or removed, an entity destroyed).
    template <class T> T *TryGet(Entity entity) noexcept {
        return static_cast<T *>(GetMutableRaw(entity, ComponentIdOf<T>()));
    }
    template <class T> const T *TryGet(Entity entity) const noexcept {
        return static_cast<const T *>(GetRaw(entity, ComponentIdOf<T>()));
    }

    // The entity's value; the entity must have the component.
    template <class T> T &Get(Entity entity) noexcept {
        T *value = TryGet<T>(entity);
        RUNIC_ASSERT_MSG(value != nullptr, "the entity does not have the component");
        return *value;
    }
    template <class T> const T &Get(Entity entity) const noexcept {
        const T *value = TryGet<T>(entity);
        RUNIC_ASSERT_MSG(value != nullptr, "the entity does not have the component");
        return *value;
    }

    template <class T> bool Has(Entity entity) const noexcept {
        return HasRaw(entity, ComponentIdOf<T>());
    }

    // Takes a component from an entity; nothing happens if it lacks it.
    template <class T> void Remove(Entity entity) { RemoveRaw(entity, ComponentIdOf<T>()); }

    // ---- Queries -------------------------------------------------------------------------------

    // Makes a cached query for the entities that have every Ts, named for tools. Made at setup
    // and kept; the world owns it. A const type is read, any other may be written.
    template <class... Ts> EntityQuery<Ts...> MakeQuery(std::string_view name);

    std::uint32_t QueryCount() const noexcept;
    QueryInfo DescribeQuery(std::uint32_t index) const;

    // ---- Holding changes back ------------------------------------------------------------------

    // While a DeferScope lives, Set, Remove, Destroy and the bookkeeping of Create are held
    // back; they apply, in the order they were asked, when the outermost scope (or query
    // iteration) ends. Reads see the world as it was, and a value written through Get or a
    // query's reference changes at once. A phase holds one over its systems so the world's
    // shape is stable while they run.
    class DeferScope {
      public:
        explicit DeferScope(EntityWorld &world) : m_World(world) { m_World.BeginDeferred(); }
        ~DeferScope() { m_World.EndDeferred(); }

        DeferScope(const DeferScope &) = delete;
        DeferScope &operator=(const DeferScope &) = delete;

      private:
        EntityWorld &m_World;
    };

    bool IsDeferred() const noexcept;

    // ---- For tools -----------------------------------------------------------------------------

    // Every entity created through this world, in no particular order.
    void ForEachEntity(const std::function<void(Entity)> &visit) const;
    // Every registered component the entity has, with its value.
    void ForEachComponentOf(
        Entity entity,
        const std::function<void(const ComponentInfo &, const void *component)> &visit) const;
    // The set of components the entity has now.
    ArchetypeId ArchetypeOf(Entity entity) const noexcept;

    // ---- For Runic::ECSNative only -------------------------------------------------------------

    // The library's own world object and component id. Only the ECSNative headers use them.
    void *NativeWorldHandle() const noexcept;
    template <class T> std::uint64_t ComponentIdOf() const noexcept {
        const std::uint32_t index = Detail::TypeIndexOf<std::remove_cv_t<T>>();
        RUNIC_ASSERT_MSG(index < m_ComponentIdCount && m_ComponentIds[index] != kNoComponent,
                         "the component type is not registered in this world");
        return m_ComponentIds[index];
    }

  private:
    template <class... Ts> friend class EntityQuery;

    static constexpr std::uint64_t kNoComponent = 0;

    void RegisterRaw(const Detail::ComponentRegistration &registration);
    void SetRaw(Entity entity, std::uint64_t component, std::size_t size, const void *value);
    void *GetMutableRaw(Entity entity, std::uint64_t component) noexcept;
    const void *GetRaw(Entity entity, std::uint64_t component) const noexcept;
    bool HasRaw(Entity entity, std::uint64_t component) const noexcept;
    void RemoveRaw(Entity entity, std::uint64_t component);

    std::uint32_t MakeQueryRaw(std::string_view name, std::span<const Detail::QueryTerm> terms);
    void IterateQuery(std::uint32_t query, Detail::ChunkVisitor visitor, void *user);
    std::uint32_t CountQuery(std::uint32_t query) const;

    void BeginDeferred();
    void EndDeferred();

    struct State;
    std::unique_ptr<State> m_State;

    // The library's id of each registered component type, by Detail::TypeIndexOf; kNoComponent
    // where the type is not registered. Kept here so the lookup is inline.
    const std::uint64_t *m_ComponentIds = nullptr;
    std::uint32_t m_ComponentIdCount = 0;
};

// ---- Templates ---------------------------------------------------------------------------------

template <class T>
void EntityWorld::RegisterComponent(std::string_view name, const ComponentOptions &options) {
    static_assert(!std::is_const_v<T> && !std::is_reference_v<T>, "register the plain type");
    static_assert(std::is_default_constructible_v<T>, "a component is default-constructible");
    static_assert(std::is_copy_assignable_v<T> && std::is_move_assignable_v<T>,
                  "a component is copy- and move-assignable");
    Detail::ComponentRegistration registration;
    registration.typeIndex = Detail::TypeIndexOf<T>();
    registration.name = name;
    registration.options = options;
    registration.size = sizeof(T);
    registration.alignment = alignof(T);
    registration.lifecycle = Detail::LifecycleOf<T>();
    RegisterRaw(registration);
}

template <class... Ts> EntityQuery<Ts...> EntityWorld::MakeQuery(std::string_view name) {
    static_assert(sizeof...(Ts) > 0 && sizeof...(Ts) <= kMaxQueryTerms,
                  "a query names 1 to kMaxQueryTerms components");
    const std::array<Detail::QueryTerm, sizeof...(Ts)> terms{
        Detail::QueryTerm{Detail::TypeIndexOf<std::remove_cv_t<Ts>>(), std::is_const_v<Ts>}...};
    return EntityQuery<Ts...>(*this, MakeQueryRaw(name, terms));
}

template <class... Ts>
template <class Visit, std::size_t... Columns>
void EntityQuery<Ts...>::VisitChunk(Visit &visit, const Detail::QueryChunk &chunk,
                                    std::index_sequence<Columns...>) {
    visit(std::span<const Entity>(chunk.entities, chunk.count),
          std::span<Ts>(static_cast<Ts *>(chunk.columns[Columns]), chunk.count)...);
}

template <class... Ts>
template <class Visit>
void EntityQuery<Ts...>::EachChunk(Visit &&visit) const {
    RUNIC_ASSERT_MSG(IsValid(), "a query of nothing was iterated");
    using Stored = std::remove_reference_t<Visit>;
    m_World->IterateQuery(
        m_Index,
        [](void *user, const Detail::QueryChunk &chunk) {
            VisitChunk(*static_cast<Stored *>(user), chunk, std::index_sequence_for<Ts...>{});
        },
        const_cast<void *>(static_cast<const void *>(std::addressof(visit))));
}

template <class... Ts> template <class Visit> void EntityQuery<Ts...>::Each(Visit &&visit) const {
    EachChunk([&visit](std::span<const Entity> entities, std::span<Ts>... values) {
        for (std::size_t row = 0; row < entities.size(); ++row) {
            visit(entities[row], values[row]...);
        }
    });
}

template <class... Ts> std::uint32_t EntityQuery<Ts...>::Count() const {
    RUNIC_ASSERT_MSG(IsValid(), "a query of nothing was counted");
    return m_World->CountQuery(m_Index);
}

} // namespace Runic::ECS
