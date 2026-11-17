#pragma once

#include <compare>
#include <cstddef>
#include <cstdint>
#include <functional>

// An entity of one EntityWorld. The handle is local to the process that made it: it never goes
// into a message, a file or a service call. Across processes an entity is known by its
// Core::GlobalEntityId (RunicDocs › Simulation core › Flecs).

namespace Runic::ECS {

class Entity {
  public:
    // The null entity: no entity at all.
    constexpr Entity() noexcept = default;

    // The handle as one number, for logs, hashing and the native door, and back.
    static constexpr Entity FromRaw(std::uint64_t raw) noexcept {
        Entity entity;
        entity.m_Raw = raw;
        return entity;
    }
    constexpr std::uint64_t Raw() const noexcept { return m_Raw; }

    constexpr bool IsNull() const noexcept { return m_Raw == kNullRaw; }
    constexpr explicit operator bool() const noexcept { return !IsNull(); }

    friend constexpr bool operator==(Entity, Entity) noexcept = default;
    friend constexpr std::strong_ordering operator<=>(Entity, Entity) noexcept = default;

  private:
    static constexpr std::uint64_t kNullRaw = 0;

    std::uint64_t m_Raw = kNullRaw;
};

// A handle is exactly its number, so a run of entities is a run of 64-bit values.
static_assert(sizeof(Entity) == sizeof(std::uint64_t));

} // namespace Runic::ECS

template <> struct std::hash<Runic::ECS::Entity> {
    std::size_t operator()(Runic::ECS::Entity entity) const noexcept {
        return std::hash<std::uint64_t>{}(entity.Raw());
    }
};
