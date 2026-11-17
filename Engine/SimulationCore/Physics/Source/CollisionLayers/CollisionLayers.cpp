// Collision layers (CollisionLayers.hpp): names, groups and a symmetric bit table.

#include "CollisionLayers.hpp"

#include "Assert.hpp"

namespace Runic::Physics {

namespace {
constexpr char kNamespaceSeparator = ':';
constexpr std::uint64_t kOneBit = 1;
} // namespace

CollisionLayer CollisionLayers::Add(std::string_view name, BroadPhaseGroup group) {
    RUNIC_ASSERT_MSG(name.find(kNamespaceSeparator) != std::string_view::npos,
                     "a collision layer's name is namespace:name");
    RUNIC_ASSERT_MSG(m_Layers.size() < kMaxCollisionLayers, "too many collision layers");
    for (const Layer &layer : m_Layers) {
        RUNIC_ASSERT_MSG(layer.name != name, "two collision layers share a name");
    }
    m_Layers.push_back({std::string(name), group});
    m_Collides.push_back(0);
    // Layer numbers start at 0; the handle keeps 0 for "none", so it carries the number plus one.
    return CollisionLayer(static_cast<std::uint16_t>(m_Layers.size()));
}

void CollisionLayers::SetCollides(CollisionLayer a, CollisionLayer b, bool collides) {
    RUNIC_ASSERT(a.IsValid() && a.Value() <= m_Layers.size());
    RUNIC_ASSERT(b.IsValid() && b.Value() <= m_Layers.size());
    const std::size_t i = a.Value() - 1u;
    const std::size_t j = b.Value() - 1u;
    if (collides) {
        m_Collides[i] |= kOneBit << j;
        m_Collides[j] |= kOneBit << i;
    } else {
        m_Collides[i] &= ~(kOneBit << j);
        m_Collides[j] &= ~(kOneBit << i);
    }
}

CollisionLayer CollisionLayers::Find(std::string_view name) const noexcept {
    for (std::size_t i = 0; i < m_Layers.size(); ++i) {
        if (m_Layers[i].name == name) {
            return CollisionLayer(static_cast<std::uint16_t>(i + 1));
        }
    }
    return CollisionLayer();
}

CollisionLayerInfo CollisionLayers::Describe(CollisionLayer layer) const {
    RUNIC_ASSERT(layer.IsValid() && layer.Value() <= m_Layers.size());
    const Layer &described = m_Layers[layer.Value() - 1u];
    return {described.name, described.group};
}

bool CollisionLayers::Collides(CollisionLayer a, CollisionLayer b) const noexcept {
    if (!a.IsValid() || !b.IsValid() || a.Value() > m_Layers.size() ||
        b.Value() > m_Layers.size()) {
        return false;
    }
    return (m_Collides[a.Value() - 1u] & (kOneBit << (b.Value() - 1u))) != 0;
}

CollisionLayers DefaultCollisionLayers() {
    CollisionLayers layers;
    const CollisionLayer still = layers.Add(kStaticLayerName, BroadPhaseGroup::Static);
    const CollisionLayer moving = layers.Add(kMovingLayerName, BroadPhaseGroup::Moving);
    const CollisionLayer character = layers.Add(kCharacterLayerName, BroadPhaseGroup::Moving);
    const CollisionLayer sensor = layers.Add(kSensorLayerName, BroadPhaseGroup::Moving);
    layers.SetCollides(still, moving, true);
    layers.SetCollides(still, character, true);
    layers.SetCollides(still, sensor, true);
    layers.SetCollides(moving, moving, true);
    layers.SetCollides(moving, character, true);
    layers.SetCollides(moving, sensor, true);
    layers.SetCollides(character, sensor, true);
    return layers;
}

} // namespace Runic::Physics
