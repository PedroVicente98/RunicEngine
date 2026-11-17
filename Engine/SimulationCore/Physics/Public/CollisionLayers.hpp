#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "PhysicsHandles.hpp"

// Collision layers and their filters (PHYS-1): which kinds of things collide. A layer has a name
// and a broad-phase group: Static layers share one broad-phase tree, never tested against each
// other, and Moving layers the other. Which pairs of layers collide is a table set before the
// PhysicsWorld is made, and fixed after. The Engine's own layers are engine: names; a game adds its
// own the same way.

namespace Runic::Physics {

enum class BroadPhaseGroup : std::uint8_t { Static, Moving };

// The most layers one table holds.
inline constexpr std::uint32_t kMaxCollisionLayers = 64;

// The Engine's layers in DefaultCollisionLayers().
inline constexpr std::string_view kStaticLayerName = "engine:static";
inline constexpr std::string_view kMovingLayerName = "engine:moving";
inline constexpr std::string_view kCharacterLayerName = "engine:character";
inline constexpr std::string_view kSensorLayerName = "engine:sensor";

struct CollisionLayerInfo {
    std::string_view name;
    BroadPhaseGroup group = BroadPhaseGroup::Moving;
};

class CollisionLayers {
  public:
    // Adds a layer; the name is "namespace:name" and unique. The first layer is number 0.
    CollisionLayer Add(std::string_view name, BroadPhaseGroup group);
    // Whether two layers collide; symmetric. A new layer collides with nothing until told.
    void SetCollides(CollisionLayer a, CollisionLayer b, bool collides);

    std::uint32_t Count() const noexcept { return static_cast<std::uint32_t>(m_Layers.size()); }
    CollisionLayer Find(std::string_view name) const noexcept;
    CollisionLayerInfo Describe(CollisionLayer layer) const;
    bool Collides(CollisionLayer a, CollisionLayer b) const noexcept;

  private:
    struct Layer {
        std::string name;
        BroadPhaseGroup group;
    };
    std::vector<Layer> m_Layers;
    std::vector<std::uint64_t> m_Collides; // one bit row per layer
};

// The Engine's four layers: static (collides with moving, character, sensor), moving (with static,
// moving, character, sensor), character (with static, moving, sensor; characters meet each other
// through their own test, not as bodies) and sensor (with moving and character).
CollisionLayers DefaultCollisionLayers();

} // namespace Runic::Physics
