// Shape instances (ShapeInstances.hpp): the writer keeps the shapes of its last list in world
// terms and compares the new list with it, index by index, as the shapes are added.

#include "RenderData/ShapeInstances.hpp"

#include <cmath>

#include "Assert.hpp"

namespace Runic::RenderData {

namespace {

// Whether an offset fits a 32-bit position: ToLocal's limit, checked here instead of asserted.
bool FitsLocal(Core::WorldDelta delta) noexcept {
    return std::abs(delta.x) < Core::kMaxLocalOffsetM &&
           std::abs(delta.y) < Core::kMaxLocalOffsetM && std::abs(delta.z) < Core::kMaxLocalOffsetM;
}

} // namespace

ShapeInstanceList::ShapeInstanceList(std::uint32_t capacity) : m_Capacity(capacity) {
    m_Instances.reserve(capacity);
    m_Changed.reserve(capacity);
}

std::span<const ShapeInstance> ShapeInstanceList::Instances() const noexcept { return m_Instances; }

std::uint64_t ShapeInstanceList::Revision() const noexcept { return m_Revision; }

std::uint64_t ShapeInstanceList::BaseRevision() const noexcept { return m_BaseRevision; }

bool ShapeInstanceList::LayoutChanged() const noexcept { return m_LayoutChanged; }

std::span<const std::uint32_t> ShapeInstanceList::Changed() const noexcept { return m_Changed; }

std::uint32_t ShapeInstanceList::Dropped() const noexcept { return m_Dropped; }

std::uint32_t ShapeInstanceList::Capacity() const noexcept { return m_Capacity; }

ShapeInstanceWriter::ShapeInstanceWriter(std::uint32_t capacity) : m_Capacity(capacity) {
    m_Previous.reserve(capacity);
    m_Current.reserve(capacity);
}

void ShapeInstanceWriter::Begin(ShapeInstanceList &list, Core::WorldPosition origin) noexcept {
    RUNIC_ASSERT_MSG(m_List == nullptr, "a shape list was begun before the last one ended");
    m_List = &list;
    m_Origin = origin;
    m_LayoutChanged = false;
    m_Current.clear();
    list.m_Instances.clear();
    list.m_Changed.clear();
    list.m_Dropped = 0;
}

bool ShapeInstanceWriter::Add(Core::RenderInstanceId id, ShapeKind shape,
                              Core::WorldPosition position, Core::Quat rotation, Core::Vec3 size,
                              PackedColor color) noexcept {
    RUNIC_ASSERT_MSG(m_List != nullptr, "a shape was added outside Begin and End");
    RUNIC_ASSERT_MSG(id.IsValid(), "a shape instance needs a valid RenderInstanceId");
    ShapeInstanceList &list = *m_List;
    const Core::WorldDelta offset = position - m_Origin;
    const std::uint32_t capacity = m_Capacity < list.m_Capacity ? m_Capacity : list.m_Capacity;
    if (m_Current.size() >= capacity || !FitsLocal(offset)) {
        ++list.m_Dropped;
        return false;
    }

    const auto index = static_cast<std::uint32_t>(m_Current.size());
    const Kept kept{id, shape, color, position, rotation, size};
    m_Current.push_back(kept);
    list.m_Instances.push_back({id, shape, color, Core::ToLocal(offset), rotation, size});

    if (index >= m_Previous.size() || m_Previous[index].id != id) {
        m_LayoutChanged = true; // a new shape, or another one at this index
        return true;
    }
    const Kept &before = m_Previous[index];
    const bool same = before.shape == shape && before.color == color &&
                      before.position == position && before.rotation == rotation &&
                      before.size == size;
    if (!same) {
        list.m_Changed.push_back(index);
    }
    return true;
}

void ShapeInstanceWriter::End() noexcept {
    RUNIC_ASSERT_MSG(m_List != nullptr, "a shape list was ended before it was begun");
    ShapeInstanceList &list = *m_List;
    m_List = nullptr;
    if (m_Current.size() != m_Previous.size()) {
        m_LayoutChanged = true; // shapes left the end of the list
    }
    const std::uint64_t base = m_Revision;
    if (m_LayoutChanged || !list.m_Changed.empty()) {
        ++m_Revision;
    }
    list.m_Revision = m_Revision;
    list.m_BaseRevision = base;
    list.m_LayoutChanged = m_LayoutChanged;
    if (m_LayoutChanged) {
        // The indices of the base mean nothing now; every shape is new to an index-based reader.
        list.m_Changed.clear();
    }
    m_Previous.swap(m_Current); // both keep their storage
}

std::uint64_t ShapeInstanceWriter::Revision() const noexcept { return m_Revision; }

std::uint32_t ShapeInstanceWriter::Capacity() const noexcept { return m_Capacity; }

} // namespace Runic::RenderData
