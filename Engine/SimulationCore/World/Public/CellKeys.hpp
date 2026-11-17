#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>

#include "PlainIds.hpp"

// The keys of a world's partition (RunicDocs › World space coordinates, cells and activation ›
// World hierarchy). A cell is a cube of the world addressed by three 32-bit integers, y included:
// a cave under a town or an island in the sky has a cell of its own. A sector is a coarse bucket
// of cells on the ground plane. Cells are sparse: one exists only where something is.

namespace Runic::World {

// A cell's address in its world: the cell that starts at origin + (x, y, z) × the cell size.
struct CellCoord {
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::int32_t z = 0;

    friend constexpr bool operator==(const CellCoord &, const CellCoord &) noexcept = default;
};

// A sector's address in its world: a column of cells, every y, sectors-per-cell wide on x and z.
struct SectorCoord {
    std::int32_t x = 0;
    std::int32_t z = 0;

    friend constexpr bool operator==(const SectorCoord &, const SectorCoord &) noexcept = default;
};

// A cell of a world: what streaming, the cooker and the network address a piece of the world by.
struct CellKey {
    Core::WorldId world;
    CellCoord cell;

    friend constexpr bool operator==(const CellKey &, const CellKey &) noexcept = default;
};

struct SectorKey {
    Core::WorldId world;
    SectorCoord sector;

    friend constexpr bool operator==(const SectorKey &, const SectorKey &) noexcept = default;
};

namespace Detail {
// Mixes one more number into a hash.
constexpr std::size_t HashCombine(std::size_t seed, std::uint64_t value) noexcept {
    constexpr std::uint64_t kMix = 0x9E37'79B9'7F4A'7C15;
    constexpr int kLeft = 6;
    constexpr int kRight = 2;
    return seed ^ static_cast<std::size_t>(value + kMix + (seed << kLeft) + (seed >> kRight));
}
} // namespace Detail

} // namespace Runic::World

template <> struct std::hash<Runic::World::CellCoord> {
    std::size_t operator()(const Runic::World::CellCoord &cell) const noexcept {
        using Runic::World::Detail::HashCombine;
        return HashCombine(
            HashCombine(static_cast<std::uint32_t>(cell.x), static_cast<std::uint32_t>(cell.y)),
            static_cast<std::uint32_t>(cell.z));
    }
};

template <> struct std::hash<Runic::World::CellKey> {
    std::size_t operator()(const Runic::World::CellKey &key) const noexcept {
        return Runic::World::Detail::HashCombine(std::hash<Runic::World::CellCoord>{}(key.cell),
                                                 key.world.Value());
    }
};

template <> struct std::hash<Runic::World::SectorKey> {
    std::size_t operator()(const Runic::World::SectorKey &key) const noexcept {
        using Runic::World::Detail::HashCombine;
        return HashCombine(HashCombine(static_cast<std::uint32_t>(key.sector.x),
                                       static_cast<std::uint32_t>(key.sector.z)),
                           key.world.Value());
    }
};
