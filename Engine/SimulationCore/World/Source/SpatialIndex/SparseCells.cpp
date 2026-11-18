// The sparse 3D grid (SparseCells.hpp).

#include "SpatialIndex/SparseCells.hpp"

#include "Assert.hpp"

namespace Runic::World::Detail {

void SparseCells::Add(const CellRange &range, Slot slot) {
    for (std::int64_t x = range.min.x; x <= range.max.x; ++x) {
        for (std::int64_t z = range.min.z; z <= range.max.z; ++z) {
            for (std::int64_t y = range.min.y; y <= range.max.y; ++y) {
                Add(CellCoord{static_cast<std::int32_t>(x), static_cast<std::int32_t>(y),
                              static_cast<std::int32_t>(z)},
                    slot);
            }
        }
    }
}

void SparseCells::Remove(const CellRange &range, Slot slot) {
    for (std::int64_t x = range.min.x; x <= range.max.x; ++x) {
        for (std::int64_t z = range.min.z; z <= range.max.z; ++z) {
            for (std::int64_t y = range.min.y; y <= range.max.y; ++y) {
                Remove(CellCoord{static_cast<std::int32_t>(x), static_cast<std::int32_t>(y),
                                 static_cast<std::int32_t>(z)},
                       slot);
            }
        }
    }
}

void SparseCells::Add(CellCoord cell, Slot slot) {
    Column &column = m_Columns[KeyOf(cell.x, cell.z)];
    auto at = std::lower_bound(column.cells.begin(), column.cells.end(), cell.y,
                               [](const YCell &c, std::int32_t y) { return c.y < y; });
    if (at == column.cells.end() || at->y != cell.y) {
        std::uint32_t index = 0;
        if (m_FreeCells.empty()) {
            index = static_cast<std::uint32_t>(m_Cells.size());
            m_Cells.emplace_back();
        } else {
            index = m_FreeCells.back();
            m_FreeCells.pop_back();
        }
        at = column.cells.insert(at, YCell{cell.y, index});
    }
    m_Cells[at->cell].push_back(slot);
    ++m_Entries;
}

void SparseCells::Remove(CellCoord cell, Slot slot) {
    const auto column = m_Columns.find(KeyOf(cell.x, cell.z));
    RUNIC_ASSERT_MSG(column != m_Columns.end(), "the spatial grid has no such column");
    std::vector<YCell> &cells = column->second.cells;
    const auto at = std::lower_bound(cells.begin(), cells.end(), cell.y,
                                     [](const YCell &c, std::int32_t y) { return c.y < y; });
    RUNIC_ASSERT_MSG(at != cells.end() && at->y == cell.y, "the spatial grid has no such cell");
    std::vector<Slot> &slots = m_Cells[at->cell];
    const auto found = std::find(slots.begin(), slots.end(), slot);
    RUNIC_ASSERT_MSG(found != slots.end(), "the cell does not hold the slot");
    slots.erase(found);
    --m_Entries;
    if (slots.empty()) {
        m_FreeCells.push_back(at->cell);
        cells.erase(at);
        if (cells.empty()) {
            m_Columns.erase(column);
        }
    }
}

std::span<const SparseCells::Slot> SparseCells::SlotsIn(CellCoord cell) const noexcept {
    const auto column = m_Columns.find(KeyOf(cell.x, cell.z));
    if (column == m_Columns.end()) {
        return {};
    }
    const std::vector<YCell> &cells = column->second.cells;
    const auto at = std::lower_bound(cells.begin(), cells.end(), cell.y,
                                     [](const YCell &c, std::int32_t y) { return c.y < y; });
    if (at == cells.end() || at->y != cell.y) {
        return {};
    }
    return m_Cells[at->cell];
}

std::uint32_t SparseCells::Cells() const noexcept {
    return static_cast<std::uint32_t>(m_Cells.size() - m_FreeCells.size());
}

} // namespace Runic::World::Detail
