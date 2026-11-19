#pragma once

#include "Math.hpp"

// The 64-bit world (RunicDocs › World space coordinates, cells and activation › Which values are
// 64-bit): a point in the world is 64-bit and everything else is 32-bit; going up is implicit, and
// going down happens only through ToLocal. Positions are metres with a real Y axis; the smallest
// unit gameplay relies on is 1 cm.

namespace Runic::Core {

// A place in the world, in metres; y is real altitude or depth.
struct WorldPosition {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;

    friend constexpr bool operator==(const WorldPosition &,
                                     const WorldPosition &) noexcept = default;
};

// The difference between two places; it may span 100 km.
struct WorldDelta {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;

    friend constexpr bool operator==(const WorldDelta &, const WorldDelta &) noexcept = default;
};

constexpr WorldDelta operator-(WorldPosition a, WorldPosition b) noexcept {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}
constexpr WorldPosition operator+(WorldPosition p, WorldDelta d) noexcept {
    return {p.x + d.x, p.y + d.y, p.z + d.z};
}
constexpr WorldPosition operator-(WorldPosition p, WorldDelta d) noexcept {
    return {p.x - d.x, p.y - d.y, p.z - d.z};
}
constexpr WorldDelta operator+(WorldDelta a, WorldDelta b) noexcept {
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}
constexpr WorldDelta operator-(WorldDelta a, WorldDelta b) noexcept {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}
constexpr WorldDelta operator*(WorldDelta d, double s) noexcept {
    return {d.x * s, d.y * s, d.z * s};
}

// A 32-bit local offset goes up with no loss.
inline WorldPosition operator+(WorldPosition p, Vec3 local) noexcept {
    return {p.x + local.x, p.y + local.y, p.z + local.z};
}

constexpr double DistanceSq(WorldPosition a, WorldPosition b) noexcept {
    const WorldDelta d = a - b;
    return d.x * d.x + d.y * d.y + d.z * d.z;
}

// Not defined, so writing them does not compile: WorldPosition + WorldPosition, WorldPosition *
// double, and any conversion of a WorldPosition or a WorldDelta to Vec3.

// The longest offset ToLocal accepts: at 10 km the float step is about 1 mm.
inline constexpr double kMaxLocalOffsetM = 10'000.0;

// The only way down to 32 bits: a difference small enough for a float. Asserts that the offset is
// shorter than kMaxLocalOffsetM.
Vec3 ToLocal(WorldDelta delta) noexcept;

} // namespace Runic::Core
