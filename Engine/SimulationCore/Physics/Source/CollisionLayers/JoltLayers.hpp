#pragma once

#include <Jolt/Jolt.h>

#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>

#include "CollisionLayers.hpp"

// A CollisionLayers table as the three interfaces Jolt asks for: object layer to broad-phase layer,
// object layer against broad-phase layer, and object layer against object layer. A Runic layer
// handle n is Jolt's object layer n - 1. Private to Runic::Physics.

namespace Runic::Physics::Detail {

inline JPH::ObjectLayer ToJolt(CollisionLayer layer) noexcept {
    return static_cast<JPH::ObjectLayer>(layer.Value() - 1u);
}
inline CollisionLayer FromJolt(JPH::ObjectLayer layer) noexcept {
    return CollisionLayer(static_cast<std::uint16_t>(layer + 1u));
}

inline constexpr std::uint32_t kBroadPhaseLayerCount = 2; // Static and Moving

class JoltLayers final : public JPH::BroadPhaseLayerInterface,
                         public JPH::ObjectVsBroadPhaseLayerFilter,
                         public JPH::ObjectLayerPairFilter {
  public:
    explicit JoltLayers(const CollisionLayers &layers) : m_Layers(layers) {
        // Jolt asks this for every broad-phase query: a mask per layer, worked out once.
        for (std::uint32_t self = 1; self <= m_Layers.Count(); ++self) {
            std::uint8_t trees = 0;
            for (std::uint32_t other = 1; other <= m_Layers.Count(); ++other) {
                const CollisionLayer a(static_cast<std::uint16_t>(self));
                const CollisionLayer b(static_cast<std::uint16_t>(other));
                if (m_Layers.Collides(a, b)) {
                    trees |= static_cast<std::uint8_t>(
                        kTreeBit << static_cast<std::uint32_t>(m_Layers.Describe(b).group));
                }
            }
            m_Trees[self - 1u] = trees;
        }
    }

    const CollisionLayers &Table() const noexcept { return m_Layers; }

    // BroadPhaseLayerInterface
    JPH::uint GetNumBroadPhaseLayers() const override { return kBroadPhaseLayerCount; }
    JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer layer) const override {
        return JPH::BroadPhaseLayer(
            static_cast<JPH::BroadPhaseLayer::Type>(m_Layers.Describe(FromJolt(layer)).group));
    }
#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
    const char *GetBroadPhaseLayerName(JPH::BroadPhaseLayer layer) const override {
        return static_cast<BroadPhaseGroup>(static_cast<JPH::BroadPhaseLayer::Type>(layer)) ==
                       BroadPhaseGroup::Static
                   ? "static"
                   : "moving";
    }
#endif

    // ObjectVsBroadPhaseLayerFilter: a layer meets a broad-phase tree when it collides with any
    // layer in it.
    bool ShouldCollide(JPH::ObjectLayer layer, JPH::BroadPhaseLayer tree) const override {
        const auto group =
            static_cast<std::uint32_t>(static_cast<JPH::BroadPhaseLayer::Type>(tree));
        return (m_Trees[layer] & (kTreeBit << group)) != 0;
    }

    // ObjectLayerPairFilter
    bool ShouldCollide(JPH::ObjectLayer a, JPH::ObjectLayer b) const override {
        return m_Layers.Collides(FromJolt(a), FromJolt(b));
    }

  private:
    static constexpr std::uint32_t kTreeBit = 1;

    CollisionLayers m_Layers;
    std::uint8_t m_Trees[kMaxCollisionLayers] = {}; // per layer: the broad-phase trees it meets
};

} // namespace Runic::Physics::Detail
