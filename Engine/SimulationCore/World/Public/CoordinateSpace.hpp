#pragma once

#include <cstdint>

#include "CellKeys.hpp"
#include "Math.hpp"
#include "WorldSpace.hpp"

// The coordinate space of a world: where its cell grid starts and how large its cells and sectors
// are. The sizes are whole metres, so every cell boundary is an exact 64-bit value however far
// from the origin, and a position always falls in exactly one cell (RunicDocs › World space
// coordinates, cells and activation: a cell key is (x, y, z) at a fixed size per world).

namespace Runic::World {

// The sizes RunicDocs uses as examples: 64 m cells, 512 m sectors. A game sets its own per world.
inline constexpr std::uint32_t kDefaultCellSizeM = 64;
inline constexpr std::uint32_t kDefaultCellsPerSector = 8;

struct CoordinateSpace {
    Core::WorldPosition origin; // the lowest corner of cell (0, 0, 0)
    std::uint32_t cellSizeM = kDefaultCellSizeM;
    std::uint32_t cellsPerSector = kDefaultCellsPerSector;

    double SectorSizeM() const noexcept {
        return static_cast<double>(cellSizeM) * static_cast<double>(cellsPerSector);
    }

    // The cell a position is in: floor((position − origin) / cell size) on each axis, at any
    // altitude or depth. The position is within the range 32-bit cell addresses cover.
    CellCoord CellOf(Core::WorldPosition position) const noexcept;

    // The lowest corner and the middle of a cell. Exact: CellOf(CellMin(c)) == c.
    Core::WorldPosition CellMin(CellCoord cell) const noexcept;
    Core::WorldPosition CellCenter(CellCoord cell) const noexcept;

    // Where a position is inside its cell: each axis from 0 up to the cell size, as 32-bit
    // floats. CellMin(CellOf(p)) + OffsetInCell(p) is p to within a float's step at that size
    // (4 µm in a 64 m cell).
    Core::Vec3 OffsetInCell(Core::WorldPosition position) const noexcept;

    // The sector a cell, or a position, is in.
    SectorCoord SectorOf(CellCoord cell) const noexcept;
    SectorCoord SectorOf(Core::WorldPosition position) const noexcept {
        return SectorOf(CellOf(position));
    }

    friend constexpr bool operator==(const CoordinateSpace &,
                                     const CoordinateSpace &) noexcept = default;
};

} // namespace Runic::World
