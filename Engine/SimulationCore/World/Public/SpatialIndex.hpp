#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "CellKeys.hpp"
#include "CoordinateSpace.hpp"
#include "Entity.hpp"
#include "Math.hpp"
#include "WorldSpace.hpp"

// The spatial index (WORLD-2; RunicDocs › World space coordinates, cells and activation › Sparse
// 3D cells and exact queries). A world's cells are 3D and sparse by column: each XZ column holds
// only the Y cells that have members, so empty air and solid rock cost nothing, and a cave under a
// town or an island above it has cells of its own. A member is an entity with a box around a
// 64-bit position; it is in every cell its box touches, so a ship as large as many cells is in all
// of them.
//
// A query gathers the cells its volume's box touches and tests every member there exactly, in 3D
// and in 64-bit: a sphere, a box or an ellipsoid (wider than tall, as a player's surroundings
// are). A 2D distance is never the answer. What it returns is what testing every member of the
// index would return, each member once.

namespace Runic::World {

// Where a member is: a box around a 64-bit center, its half sizes 32-bit (all zero: a point).
struct SpatialBounds {
    Core::WorldPosition center;
    Core::Vec3 halfExtents{0.0f};
};

// The box of cells from min to max, both included.
struct CellRange {
    CellCoord min;
    CellCoord max;

    bool Contains(CellCoord cell) const noexcept {
        return cell.x >= min.x && cell.x <= max.x && cell.y >= min.y && cell.y <= max.y &&
               cell.z >= min.z && cell.z <= max.z;
    }
    std::uint64_t Count() const noexcept;

    friend constexpr bool operator==(const CellRange &, const CellRange &) noexcept = default;
};

// The cells a box touches in a coordinate space: from the cell of its lowest corner to the cell of
// its highest, boundaries included.
CellRange CellsTouched(const CoordinateSpace &space, const SpatialBounds &bounds) noexcept;

enum class QueryShape : std::uint8_t { Sphere, Box, Ellipsoid };

// A query's volume around a 64-bit center. extents is the radius on every axis for a sphere, the
// half sizes for a box and the radii along x, y and z for an ellipsoid; all are above zero.
struct SpatialQuery {
    QueryShape shape = QueryShape::Sphere;
    Core::WorldPosition center;
    Core::Vec3 extents{0.0f};

    static SpatialQuery Sphere(Core::WorldPosition center, float radius) noexcept;
    static SpatialQuery Box(Core::WorldPosition center, Core::Vec3 halfExtents) noexcept;
    static SpatialQuery Ellipsoid(Core::WorldPosition center, Core::Vec3 radii) noexcept;

    // The exact test the index applies to every candidate: true when the member's box and the
    // volume share at least one point, boundaries included. Computed in 64-bit from the offset
    // between the two centers, so it is as exact 8,000 km out as at the origin.
    bool Overlaps(const SpatialBounds &bounds) const noexcept;
    bool Contains(Core::WorldPosition point) const noexcept { return Overlaps({point}); }
    // The box around the volume.
    SpatialBounds Bounds() const noexcept { return {center, extents}; }
};

struct SpatialIndexStats {
    std::uint32_t members = 0;
    std::uint32_t cells = 0;       // occupied cells
    std::uint32_t columns = 0;     // XZ columns with at least one occupied cell
    std::uint64_t memberships = 0; // (member, cell) pairs: a member in n cells counts n
    std::uint64_t cellChanges = 0; // cells entered or left by updates since the index was made
};

struct SpatialCellInfo {
    CellCoord cell;
    std::uint32_t members = 0;
};

class SpatialIndex {
  public:
    explicit SpatialIndex(const CoordinateSpace &space = {});
    ~SpatialIndex();

    SpatialIndex(const SpatialIndex &) = delete;
    SpatialIndex &operator=(const SpatialIndex &) = delete;

    const CoordinateSpace &Space() const noexcept;

    // ---- Membership ----------------------------------------------------------------------------
    // An entity is a member once (asserted).
    void Insert(ECS::Entity member, const SpatialBounds &bounds);
    // Moves a member: only the cells it left and the cells it entered change, and a move that
    // stays in the same cells changes none. True when its cells changed.
    bool Update(ECS::Entity member, const SpatialBounds &bounds);
    void Remove(ECS::Entity member);
    bool Contains(ECS::Entity member) const noexcept;
    // A member's bounds and cells (asserted a member).
    const SpatialBounds &BoundsOf(ECS::Entity member) const;
    CellRange CellsOf(ECS::Entity member) const;

    // ---- Queries -------------------------------------------------------------------------------
    // Appends to out every member whose box the query's volume overlaps, once each. They come in
    // cell order (x, then z, then y, ascending) and, in a cell, in the order they entered it; a
    // member in several of the cells is reported from the first of them. A query reads the index
    // only, so several threads may query at once while nothing changes it.
    void Query(const SpatialQuery &query, std::vector<ECS::Entity> &out) const;

    // ---- For tools and the debug view ----------------------------------------------------------
    std::uint32_t MembersIn(CellCoord cell) const noexcept;
    // The occupied cells inside a range, in cell order.
    void Cells(const CellRange &range, std::vector<SpatialCellInfo> &out) const;
    // The members of one cell, in the order they entered it.
    void MembersOf(CellCoord cell, std::vector<ECS::Entity> &out) const;
    SpatialIndexStats Stats() const noexcept;

  private:
    struct State;
    std::unique_ptr<State> m_State;
};

} // namespace Runic::World
