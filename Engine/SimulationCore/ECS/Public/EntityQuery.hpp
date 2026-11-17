#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <type_traits>
#include <utility>

#include "Entity.hpp"

// A query: every entity that has all of a set of components. Queries are cached: a world keeps
// each one up to date as entities change, so recurring work asks for its entities without
// searching (RunicDocs › Simulation core › Flecs: "Recurring work uses cached queries"). Make a
// query once, at setup, and keep it; EntityWorld::MakeQuery returns it.

namespace Runic::ECS {

class EntityWorld;

// The most components one query names.
inline constexpr std::size_t kMaxQueryTerms = 8;

// A query as tools list it. The text stays valid while the world lives.
struct QueryInfo {
    std::string_view name;
    std::uint32_t terms = 0;
};

namespace Detail {

// One component of a query: the type and whether the query may write it.
struct QueryTerm {
    std::uint32_t typeIndex = 0;
    bool readOnly = false;
};

// A run of entities that are stored together: for each of the query's components, a contiguous
// array with one value per entity, in the same order.
struct QueryChunk {
    std::uint32_t count = 0;
    const Entity *entities = nullptr;
    std::array<void *, kMaxQueryTerms> columns{};
};

using ChunkVisitor = void (*)(void *user, const QueryChunk &chunk);

} // namespace Detail

// A handle to a query a world owns: cheap to copy, valid while the world lives. Ts are the
// component types; a const type is read, any other may be written.
template <class... Ts> class EntityQuery {
  public:
    // A query of nothing: MakeQuery gives the real ones.
    EntityQuery() noexcept = default;

    bool IsValid() const noexcept { return m_World != nullptr; }

    // Calls visit(Entity, Ts &...) for every matching entity. While it runs, the world holds
    // changes back (EntityWorld::DeferScope): Set, Remove, Destroy and the bookkeeping of Create
    // apply when the outermost Each, EachChunk or DeferScope ends. A value written through the
    // references visit receives changes at once.
    template <class Visit> void Each(Visit &&visit) const;

    // Calls visit(std::span<const Entity>, std::span<Ts>...) once per run of entities stored
    // together: the form for tight loops. The same rule for changes applies.
    template <class Visit> void EachChunk(Visit &&visit) const;

    // How many entities match now.
    std::uint32_t Count() const;

  private:
    friend class EntityWorld;

    EntityQuery(EntityWorld &world, std::uint32_t index) noexcept
        : m_World(&world), m_Index(index) {}

    template <class Visit, std::size_t... Columns>
    static void VisitChunk(Visit &visit, const Detail::QueryChunk &chunk,
                           std::index_sequence<Columns...>);

    EntityWorld *m_World = nullptr;
    std::uint32_t m_Index = 0;
};

} // namespace Runic::ECS
