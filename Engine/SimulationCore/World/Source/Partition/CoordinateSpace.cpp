// A world's cells and sectors (CoordinateSpace.hpp).

#include "CoordinateSpace.hpp"

#include <cmath>
#include <limits>

#include "Assert.hpp"

namespace Runic::World {

namespace {

constexpr double kHalf = 0.5;

// The cell index of one axis: how many whole cells the coordinate is past the origin.
std::int32_t CellIndex(double coordinate, double origin, std::uint32_t cellSizeM) noexcept {
    const double index = std::floor((coordinate - origin) / static_cast<double>(cellSizeM));
    RUNIC_ASSERT_MSG(index >= static_cast<double>(std::numeric_limits<std::int32_t>::min()) &&
                         index <= static_cast<double>(std::numeric_limits<std::int32_t>::max()),
                     "a position is outside the range 32-bit cell addresses cover");
    return static_cast<std::int32_t>(index);
}

// Floor division: the bucket of size `size` an index is in, also for negative indices.
std::int32_t Bucket(std::int32_t index, std::uint32_t size) noexcept {
    const auto wide = static_cast<std::int64_t>(index);
    const auto divisor = static_cast<std::int64_t>(size);
    const std::int64_t quotient = wide / divisor;
    return static_cast<std::int32_t>(wide % divisor < 0 ? quotient - 1 : quotient);
}

} // namespace

CellCoord CoordinateSpace::CellOf(Core::WorldPosition position) const noexcept {
    RUNIC_ASSERT(cellSizeM > 0);
    return CellCoord{CellIndex(position.x, origin.x, cellSizeM),
                     CellIndex(position.y, origin.y, cellSizeM),
                     CellIndex(position.z, origin.z, cellSizeM)};
}

Core::WorldPosition CoordinateSpace::CellMin(CellCoord cell) const noexcept {
    const auto size = static_cast<double>(cellSizeM);
    return Core::WorldPosition{origin.x + static_cast<double>(cell.x) * size,
                               origin.y + static_cast<double>(cell.y) * size,
                               origin.z + static_cast<double>(cell.z) * size};
}

Core::WorldPosition CoordinateSpace::CellCenter(CellCoord cell) const noexcept {
    const double half = static_cast<double>(cellSizeM) * kHalf;
    const Core::WorldPosition min = CellMin(cell);
    return Core::WorldPosition{min.x + half, min.y + half, min.z + half};
}

Core::Vec3 CoordinateSpace::OffsetInCell(Core::WorldPosition position) const noexcept {
    return Core::ToLocal(position - CellMin(CellOf(position)));
}

SectorCoord CoordinateSpace::SectorOf(CellCoord cell) const noexcept {
    RUNIC_ASSERT(cellsPerSector > 0);
    return SectorCoord{Bucket(cell.x, cellsPerSector), Bucket(cell.z, cellsPerSector)};
}

} // namespace Runic::World
