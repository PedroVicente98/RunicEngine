#pragma once

#include <compare>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <type_traits>

#include "Assert.hpp"

// The plain ID types every module and process shares. Core defines the types; the Game's data and
// the services give the values. Local Flecs entity IDs never leave a process: across processes,
// entities and players are GlobalEntityId and GlobalPlayerId.

namespace Runic::Core {

// An opaque ID: a distinct type per kind (Tag), built only explicitly from its integer, and 0
// (kInvalidValue) is invalid. IDs compare and hash by value, and never convert into another kind,
// or back to an integer, implicitly.
template <class Tag, class Rep> class Id {
    static_assert(std::is_unsigned_v<Rep>, "an ID's representation is an unsigned integer");

  public:
    using ValueType = Rep;
    // The value of the invalid ID; every other value is valid.
    static constexpr Rep kInvalidValue = 0;

    constexpr Id() noexcept = default; // the invalid ID
    constexpr explicit Id(Rep value) noexcept : m_Value(value) {}

    static constexpr Id Invalid() noexcept { return Id(); }
    constexpr Rep Value() const noexcept { return m_Value; }
    constexpr bool IsValid() const noexcept { return m_Value != kInvalidValue; }

    friend constexpr bool operator==(const Id &, const Id &) noexcept = default;
    friend constexpr auto operator<=>(const Id &, const Id &) noexcept = default;

  private:
    Rep m_Value = kInvalidValue;
};

// One tag per kind; tags are never defined.
struct GlobalEntityIdTag;
struct GlobalPlayerIdTag;
struct SimulationIdTag;
struct WorldIdTag;
struct InstanceIdTag;
struct AssetIdTag;
struct WaterBodyIdTag;
struct FieldIdTag;
struct WidgetIdTag;
struct RenderInstanceIdTag;
struct DebugMeshIdTag;

// An entity across processes; never a local Flecs ID.
using GlobalEntityId = Id<GlobalEntityIdTag, std::uint64_t>;
// A player, issued by the accounts service.
using GlobalPlayerId = Id<GlobalPlayerIdTag, std::uint64_t>;
// One running simulation.
using SimulationId = Id<SimulationIdTag, std::uint32_t>;
// One seamless world: every place reachable without a loading screen.
using WorldId = Id<WorldIdTag, std::uint32_t>;
// One copy of a world.
using InstanceId = Id<InstanceIdTag, std::uint32_t>;
// A cooked asset.
using AssetId = Id<AssetIdTag, std::uint64_t>;
// A river, lake or ocean body.
using WaterBodyId = Id<WaterBodyIdTag, std::uint32_t>;
// A field grid, such as a current or a wind map.
using FieldId = Id<FieldIdTag, std::uint32_t>;
// A world-UI widget.
using WidgetId = Id<WidgetIdTag, std::uint32_t>;
// A drawn instance that the presenter keeps from frame to frame (RunicDocs › RenderData in Core ›
// Stable references): an entity's comes from its GlobalEntityId, a static object's from its cooked
// placement. Whoever fills a frame keeps the IDs in it unique.
using RenderInstanceId = Id<RenderInstanceIdTag, std::uint64_t>;
// A cached debug mesh (RunicDocs › Render data › Debug geometry: "cached debug meshes"): geometry
// uploaded once and drawn by instances. Whoever puts meshes in a library keeps the IDs unique.
using DebugMeshId = Id<DebugMeshIdTag, std::uint64_t>;

// The master tick number of a simulation. Not an opaque ID: tick 0 is a real tick (the first), and
// ticks count and only move forward.
class TickId {
  public:
    // The first tick of a simulation, at time zero.
    static constexpr std::uint64_t kFirstTick = 0;

    constexpr TickId() noexcept = default; // kFirstTick
    constexpr explicit TickId(std::uint64_t value) noexcept : m_Value(value) {}

    constexpr std::uint64_t Value() const noexcept { return m_Value; }
    constexpr TickId Next() const noexcept { return TickId(m_Value + 1); }

    friend constexpr TickId operator+(TickId tick, std::uint64_t count) noexcept {
        return TickId(tick.m_Value + count);
    }
    // The number of ticks from earlier to later; asserts that later is not before earlier.
    friend constexpr std::uint64_t operator-(TickId later, TickId earlier) noexcept {
        RUNIC_ASSERT(later.m_Value >= earlier.m_Value);
        return later.m_Value - earlier.m_Value;
    }

    friend constexpr bool operator==(const TickId &, const TickId &) noexcept = default;
    friend constexpr auto operator<=>(const TickId &, const TickId &) noexcept = default;

  private:
    std::uint64_t m_Value = kFirstTick;
};

// An authority epoch: it rises each time the owner of a region or an entity changes, so a message
// stamped with a lower epoch than the current one is stale (DistributedSimulation gives it that
// meaning, DSIM-1).
class AuthorityEpoch {
  public:
    // The epoch before any change of owner.
    static constexpr std::uint32_t kFirstEpoch = 0;

    constexpr AuthorityEpoch() noexcept = default; // kFirstEpoch
    constexpr explicit AuthorityEpoch(std::uint32_t value) noexcept : m_Value(value) {}

    constexpr std::uint32_t Value() const noexcept { return m_Value; }
    constexpr AuthorityEpoch Next() const noexcept {
        RUNIC_ASSERT(m_Value != std::numeric_limits<std::uint32_t>::max());
        return AuthorityEpoch(m_Value + 1);
    }

    friend constexpr bool operator==(const AuthorityEpoch &,
                                     const AuthorityEpoch &) noexcept = default;
    friend constexpr auto operator<=>(const AuthorityEpoch &,
                                      const AuthorityEpoch &) noexcept = default;

  private:
    std::uint32_t m_Value = kFirstEpoch;
};

} // namespace Runic::Core

// IDs hash by value, so they key std::unordered_map directly.
template <class Tag, class Rep> struct std::hash<Runic::Core::Id<Tag, Rep>> {
    std::size_t operator()(Runic::Core::Id<Tag, Rep> id) const noexcept {
        return std::hash<Rep>{}(id.Value());
    }
};
