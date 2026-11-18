#pragma once

#include <glm/gtc/quaternion.hpp>

#include "AuthoredVolumes.hpp"
#include "Math.hpp"
#include "WorldSpace.hpp"

// The geometry of volume parts in 64-bit, shared by the point test and the debug view. Private to
// Runic::World.

namespace Runic::World::Detail {

using DVec3 = glm::dvec3;
using DQuat = glm::dquat;

inline DVec3 ToDouble(Core::Vec3 v) noexcept { return DVec3(v.x, v.y, v.z); }
inline DVec3 ToDouble(Core::WorldDelta d) noexcept { return DVec3(d.x, d.y, d.z); }
inline DQuat ToDouble(Core::Quat q) noexcept {
    return glm::normalize(DQuat(static_cast<double>(q.w), static_cast<double>(q.x),
                                static_cast<double>(q.y), static_cast<double>(q.z)));
}

// A part's half sizes along its own axes, its shape's box.
DVec3 PartHalfSize(const VolumePart &part) noexcept;

// Whether a point given in the part's own space (its center at zero, its axes the axes) is in the
// part or on its surface.
bool PartContains(const VolumePart &part, DVec3 local) noexcept;

// A point in the volume's own space, from the world.
inline DVec3 ToVolumeSpace(Core::WorldPosition origin, DQuat rotation, Core::WorldPosition point) {
    return glm::conjugate(rotation) * ToDouble(point - origin);
}

// The point in a part's own space, from the volume's; untwist is the conjugate of the part's
// rotation, worked out once when the volume is added.
inline DVec3 ToPartSpace(const VolumePart &part, DQuat untwist, DVec3 inVolume) {
    return untwist * (inVolume - ToDouble(part.offset));
}

// The half sizes of the axis-aligned box around a box of half sizes half turned by rotation.
DVec3 TurnedHalfSize(DQuat rotation, DVec3 half) noexcept;

// The world box around a volume's parts, from its origin and rotation.
SpatialBounds BoundsOfParts(Core::WorldPosition origin, DQuat rotation,
                            const std::vector<VolumePart> &parts) noexcept;

} // namespace Runic::World::Detail
