#pragma once

#include "Math.hpp"
#include "WorldSpace.hpp"

// Region-local coordinates. A region (an authored content area) has a 64-bit origin; everything
// inside it is authored and cooked as 32-bit offsets from that origin, the way the RunicEditor
// shows it. Going up to the world is an addition; going down is Core's ToLocal, the one
// narrowing conversion (RunicDocs › World space coordinates, cells and activation › Each layer
// stores positions its own way).

namespace Runic::World {

// How far from the world's origin, on each axis, a region-local coordinate still goes up to the
// world and back unchanged, bit for bit, as long as it is zero or at least 1 cm (the smallest
// unit gameplay relies on): 2^23 m, about 8,388 km, minus the reach of a local offset. Further
// out a round trip is off by at most the 64-bit step there (2 nm up to 16,777 km).
inline constexpr double kExactRegionRangeM = 8'378'608.0;

struct RegionFrame {
    Core::WorldPosition origin;

    // A region-local position as a place in the world.
    Core::WorldPosition ToWorld(Core::Vec3 local) const noexcept { return origin + local; }

    // A place in the world as a region-local position. The place is less than
    // Core::kMaxLocalOffsetM (10 km) from the region's origin: ask Reaches first when unsure.
    Core::Vec3 ToRegionLocal(Core::WorldPosition position) const noexcept {
        return Core::ToLocal(position - origin);
    }

    // True when a place is near enough to the origin to have a region-local position.
    bool Reaches(Core::WorldPosition position) const noexcept;

    friend constexpr bool operator==(const RegionFrame &, const RegionFrame &) noexcept = default;
};

} // namespace Runic::World
