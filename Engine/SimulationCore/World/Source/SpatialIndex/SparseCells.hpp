#pragma once

#include <algorithm>
#include <cstdint>
#include <span>
#include <unordered_map>
#include <utility>
#include <vector>

#include "CellKeys.hpp"
#include "SpatialIndex.hpp"

// The sparse 3D grid behind the spatial index and the volumes' index: a map of XZ columns, each
// holding only its occupied Y cells, sorted by y; a cell holds slot numbers (whatever its owner
// keeps in its own slots) in the order they entered. Private to Runic::World.

namespace Runic::World::Detail {

class SparseCells {
  public:
    using Slot = std::uint32_t;

    // Puts a slot in every cell of a range, at the end of each cell's list.
    void Add(const CellRange &range, Slot slot);
    // Takes a slot out of every cell of a range; the other slots keep their order. A cell left
    // empty is freed, and a column left empty is erased.
    void Remove(const CellRange &range, Slot slot);
    void Add(CellCoord cell, Slot slot);
    void Remove(CellCoord cell, Slot slot);

    // The slots of one cell (empty when it is not occupied).
    std::span<const Slot> SlotsIn(CellCoord cell) const noexcept;

    // Calls visit(CellCoord, std::span<const Slot>) for every occupied cell of a range, in cell
    // order: x, then z, then y, ascending. A range wider than the grid's columns walks the
    // columns (and sorts the ones inside it) instead of looking up every column of the range.
    template <class Visit> void ForEachIn(const CellRange &range, Visit &&visit) const;

    std::uint32_t Cells() const noexcept;
    std::uint32_t Columns() const noexcept { return static_cast<std::uint32_t>(m_Columns.size()); }
    std::uint64_t Entries() const noexcept { return m_Entries; }

  private:
    using ColumnKey = std::uint64_t;
    struct YCell {
        std::int32_t y = 0;
        std::uint32_t cell = 0; // into m_Cells
    };
    struct Column {
        std::vector<YCell> cells; // sorted by y
    };

    static constexpr int kColumnKeyShift = 32;
    static ColumnKey KeyOf(std::int32_t x, std::int32_t z) noexcept {
        return (static_cast<ColumnKey>(static_cast<std::uint32_t>(x)) << kColumnKeyShift) |
               static_cast<std::uint32_t>(z);
    }
    static std::int32_t XOf(ColumnKey key) noexcept {
        return static_cast<std::int32_t>(static_cast<std::uint32_t>(key >> kColumnKeyShift));
    }
    static std::int32_t ZOf(ColumnKey key) noexcept {
        return static_cast<std::int32_t>(static_cast<std::uint32_t>(key));
    }

    template <class Visit>
    void VisitColumn(std::int32_t x, std::int32_t z, const Column &column, const CellRange &range,
                     Visit &visit) const;

    std::unordered_map<ColumnKey, Column> m_Columns;
    std::vector<std::vector<Slot>> m_Cells; // a freed cell keeps its memory for the next one
    std::vector<std::uint32_t> m_FreeCells;
    std::uint64_t m_Entries = 0;
};

template <class Visit>
void SparseCells::VisitColumn(std::int32_t x, std::int32_t z, const Column &column,
                              const CellRange &range, Visit &visit) const {
    auto at = std::lower_bound(column.cells.begin(), column.cells.end(), range.min.y,
                               [](const YCell &cell, std::int32_t y) { return cell.y < y; });
    for (; at != column.cells.end() && at->y <= range.max.y; ++at) {
        visit(CellCoord{x, at->y, z}, std::span<const Slot>(m_Cells[at->cell]));
    }
}

template <class Visit> void SparseCells::ForEachIn(const CellRange &range, Visit &&visit) const {
    const auto width =
        static_cast<std::uint64_t>(static_cast<std::int64_t>(range.max.x) - range.min.x + 1);
    const auto depth =
        static_cast<std::uint64_t>(static_cast<std::int64_t>(range.max.z) - range.min.z + 1);
    if (width * depth <= m_Columns.size()) {
        for (std::int64_t x = range.min.x; x <= range.max.x; ++x) {
            for (std::int64_t z = range.min.z; z <= range.max.z; ++z) {
                const auto column = m_Columns.find(
                    KeyOf(static_cast<std::int32_t>(x), static_cast<std::int32_t>(z)));
                if (column != m_Columns.end()) {
                    VisitColumn(static_cast<std::int32_t>(x), static_cast<std::int32_t>(z),
                                column->second, range, visit);
                }
            }
        }
        return;
    }
    // Wider than the grid: its columns inside the range, in (x, z) order.
    std::vector<std::pair<std::pair<std::int32_t, std::int32_t>, const Column *>> inside;
    for (const auto &[key, column] : m_Columns) {
        const std::int32_t x = XOf(key);
        const std::int32_t z = ZOf(key);
        if (x >= range.min.x && x <= range.max.x && z >= range.min.z && z <= range.max.z) {
            inside.push_back({{x, z}, &column});
        }
    }
    std::sort(inside.begin(), inside.end(),
              [](const auto &a, const auto &b) { return a.first < b.first; });
    for (const auto &[at, column] : inside) {
        VisitColumn(at.first, at.second, *column, range, visit);
    }
}

} // namespace Runic::World::Detail
