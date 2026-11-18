// The spatial index (SpatialIndex.hpp): members in the sparse grid's cells, exact queries.

#include "SpatialIndex.hpp"

#include <algorithm>
#include <cmath>
#include <unordered_map>

#include "Assert.hpp"
#include "SpatialIndex/SparseCells.hpp"

namespace Runic::World {

namespace {

constexpr double kNoExcess = 0.0;
constexpr double kUnitEllipsoid = 1.0;

// How far a member's box reaches beyond the query's center on one axis, past its own half size:
// zero when the box straddles the center's plane.
double Excess(double offset, float halfExtent) noexcept {
    return std::max(kNoExcess, std::abs(offset) - static_cast<double>(halfExtent));
}

} // namespace

std::uint64_t CellRange::Count() const noexcept {
    const auto along = [](std::int32_t low, std::int32_t high) {
        return static_cast<std::uint64_t>(static_cast<std::int64_t>(high) - low + 1);
    };
    return along(min.x, max.x) * along(min.y, max.y) * along(min.z, max.z);
}

CellRange CellsTouched(const CoordinateSpace &space, const SpatialBounds &bounds) noexcept {
    const Core::WorldDelta half{bounds.halfExtents.x, bounds.halfExtents.y, bounds.halfExtents.z};
    return {space.CellOf(bounds.center - half), space.CellOf(bounds.center + half)};
}

SpatialQuery SpatialQuery::Sphere(Core::WorldPosition center, float radius) noexcept {
    RUNIC_ASSERT_MSG(radius > 0.0f, "a query's radius is above zero");
    return {QueryShape::Sphere, center, Core::Vec3(radius)};
}

SpatialQuery SpatialQuery::Box(Core::WorldPosition center, Core::Vec3 halfExtents) noexcept {
    RUNIC_ASSERT_MSG(halfExtents.x > 0.0f && halfExtents.y > 0.0f && halfExtents.z > 0.0f,
                     "a query's half sizes are above zero");
    return {QueryShape::Box, center, halfExtents};
}

SpatialQuery SpatialQuery::Ellipsoid(Core::WorldPosition center, Core::Vec3 radii) noexcept {
    RUNIC_ASSERT_MSG(radii.x > 0.0f && radii.y > 0.0f && radii.z > 0.0f,
                     "a query's radii are above zero");
    return {QueryShape::Ellipsoid, center, radii};
}

bool SpatialQuery::Overlaps(const SpatialBounds &bounds) const noexcept {
    const Core::WorldDelta offset = bounds.center - center;
    const Core::Vec3 half = bounds.halfExtents;
    switch (shape) {
    case QueryShape::Box:
        return std::abs(offset.x) <= static_cast<double>(extents.x) + half.x &&
               std::abs(offset.y) <= static_cast<double>(extents.y) + half.y &&
               std::abs(offset.z) <= static_cast<double>(extents.z) + half.z;
    case QueryShape::Ellipsoid: {
        // In the space where the ellipsoid is a unit sphere the box stays a box, so its nearest
        // point is the clamped one, axis by axis.
        const double x = Excess(offset.x, half.x) / extents.x;
        const double y = Excess(offset.y, half.y) / extents.y;
        const double z = Excess(offset.z, half.z) / extents.z;
        return x * x + y * y + z * z <= kUnitEllipsoid;
    }
    case QueryShape::Sphere:
        break;
    }
    const double x = Excess(offset.x, half.x);
    const double y = Excess(offset.y, half.y);
    const double z = Excess(offset.z, half.z);
    const double radius = extents.x;
    return x * x + y * y + z * z <= radius * radius;
}

struct SpatialIndex::State {
    explicit State(const CoordinateSpace &of) : space(of) {}

    struct Member {
        ECS::Entity entity;
        SpatialBounds bounds;
        CellRange cells;
    };

    CoordinateSpace space;
    std::vector<Member> members; // by slot; a freed slot's entity is null
    std::vector<std::uint32_t> freeSlots;
    std::unordered_map<std::uint64_t, std::uint32_t> slotOf; // entity → slot
    Detail::SparseCells cells;
    std::uint64_t cellChanges = 0;

    std::uint32_t SlotOf(ECS::Entity entity) const {
        const auto slot = slotOf.find(entity.Raw());
        RUNIC_ASSERT_MSG(slot != slotOf.end(), "the entity is not a member of the spatial index");
        return slot->second;
    }
};

SpatialIndex::SpatialIndex(const CoordinateSpace &space)
    : m_State(std::make_unique<State>(space)) {}

SpatialIndex::~SpatialIndex() = default;

const CoordinateSpace &SpatialIndex::Space() const noexcept { return m_State->space; }

void SpatialIndex::Insert(ECS::Entity member, const SpatialBounds &bounds) {
    State &state = *m_State;
    RUNIC_ASSERT_MSG(!member.IsNull(), "the null entity cannot be a member of the spatial index");
    RUNIC_ASSERT_MSG(!state.slotOf.contains(member.Raw()),
                     "the entity is already a member of the spatial index");
    std::uint32_t slot = 0;
    if (state.freeSlots.empty()) {
        slot = static_cast<std::uint32_t>(state.members.size());
        state.members.emplace_back();
    } else {
        slot = state.freeSlots.back();
        state.freeSlots.pop_back();
    }
    State::Member &entry = state.members[slot];
    entry.entity = member;
    entry.bounds = bounds;
    entry.cells = CellsTouched(state.space, bounds);
    state.slotOf.emplace(member.Raw(), slot);
    state.cells.Add(entry.cells, slot);
}

bool SpatialIndex::Update(ECS::Entity member, const SpatialBounds &bounds) {
    State &state = *m_State;
    const std::uint32_t slot = state.SlotOf(member);
    State::Member &entry = state.members[slot];
    const CellRange before = entry.cells;
    const CellRange after = CellsTouched(state.space, bounds);
    entry.bounds = bounds;
    if (after == before) {
        return false;
    }
    // Only the cells it left and the cells it entered: in the cells it stays in, it keeps its
    // place among the others.
    for (std::int64_t x = before.min.x; x <= before.max.x; ++x) {
        for (std::int64_t z = before.min.z; z <= before.max.z; ++z) {
            for (std::int64_t y = before.min.y; y <= before.max.y; ++y) {
                const CellCoord cell{static_cast<std::int32_t>(x), static_cast<std::int32_t>(y),
                                     static_cast<std::int32_t>(z)};
                if (!after.Contains(cell)) {
                    state.cells.Remove(cell, slot);
                    ++state.cellChanges;
                }
            }
        }
    }
    for (std::int64_t x = after.min.x; x <= after.max.x; ++x) {
        for (std::int64_t z = after.min.z; z <= after.max.z; ++z) {
            for (std::int64_t y = after.min.y; y <= after.max.y; ++y) {
                const CellCoord cell{static_cast<std::int32_t>(x), static_cast<std::int32_t>(y),
                                     static_cast<std::int32_t>(z)};
                if (!before.Contains(cell)) {
                    state.cells.Add(cell, slot);
                    ++state.cellChanges;
                }
            }
        }
    }
    entry.cells = after;
    return true;
}

void SpatialIndex::Remove(ECS::Entity member) {
    State &state = *m_State;
    const std::uint32_t slot = state.SlotOf(member);
    State::Member &entry = state.members[slot];
    state.cells.Remove(entry.cells, slot);
    entry = State::Member{};
    state.slotOf.erase(member.Raw());
    state.freeSlots.push_back(slot);
}

bool SpatialIndex::Contains(ECS::Entity member) const noexcept {
    return m_State->slotOf.contains(member.Raw());
}

const SpatialBounds &SpatialIndex::BoundsOf(ECS::Entity member) const {
    return m_State->members[m_State->SlotOf(member)].bounds;
}

CellRange SpatialIndex::CellsOf(ECS::Entity member) const {
    return m_State->members[m_State->SlotOf(member)].cells;
}

void SpatialIndex::Query(const SpatialQuery &query, std::vector<ECS::Entity> &out) const {
    const State &state = *m_State;
    const CellRange range = CellsTouched(state.space, query.Bounds());
    state.cells.ForEachIn(range, [&](CellCoord cell, std::span<const std::uint32_t> slots) {
        for (const std::uint32_t slot : slots) {
            const State::Member &member = state.members[slot];
            // A member in several cells of the range is reported from the first of them only.
            const CellCoord first{std::max(member.cells.min.x, range.min.x),
                                  std::max(member.cells.min.y, range.min.y),
                                  std::max(member.cells.min.z, range.min.z)};
            if (first == cell && query.Overlaps(member.bounds)) {
                out.push_back(member.entity);
            }
        }
    });
}

std::uint32_t SpatialIndex::MembersIn(CellCoord cell) const noexcept {
    return static_cast<std::uint32_t>(m_State->cells.SlotsIn(cell).size());
}

void SpatialIndex::Cells(const CellRange &range, std::vector<SpatialCellInfo> &out) const {
    m_State->cells.ForEachIn(range, [&](CellCoord cell, std::span<const std::uint32_t> slots) {
        out.push_back({cell, static_cast<std::uint32_t>(slots.size())});
    });
}

void SpatialIndex::MembersOf(CellCoord cell, std::vector<ECS::Entity> &out) const {
    for (const std::uint32_t slot : m_State->cells.SlotsIn(cell)) {
        out.push_back(m_State->members[slot].entity);
    }
}

SpatialIndexStats SpatialIndex::Stats() const noexcept {
    const State &state = *m_State;
    SpatialIndexStats stats;
    stats.members = static_cast<std::uint32_t>(state.slotOf.size());
    stats.cells = state.cells.Cells();
    stats.columns = state.cells.Columns();
    stats.memberships = state.cells.Entries();
    stats.cellChanges = state.cellChanges;
    return stats;
}

} // namespace Runic::World
