// Cached debug meshes (DebugMeshes.hpp): the library a frame stream's writer and reader share.

#include "RenderData/DebugMeshes.hpp"

#include <utility>

namespace Runic::RenderData {

void DebugMeshLibrary::Put(Core::DebugMeshId id, std::shared_ptr<const DebugMesh> mesh) {
    const std::lock_guard lock(m_Mutex);
    ++m_Revision;
    m_Meshes[id] = Entry{id, m_Revision, std::move(mesh)};
}

void DebugMeshLibrary::Remove(Core::DebugMeshId id) {
    const std::lock_guard lock(m_Mutex);
    if (m_Meshes.erase(id) > 0) {
        ++m_Revision;
    }
}

bool DebugMeshLibrary::Contains(Core::DebugMeshId id) const {
    const std::lock_guard lock(m_Mutex);
    return m_Meshes.contains(id);
}

std::size_t DebugMeshLibrary::Count() const {
    const std::lock_guard lock(m_Mutex);
    return m_Meshes.size();
}

std::uint64_t DebugMeshLibrary::Revision() const {
    const std::lock_guard lock(m_Mutex);
    return m_Revision;
}

std::uint64_t DebugMeshLibrary::Snapshot(std::vector<Entry> &out) const {
    const std::lock_guard lock(m_Mutex);
    out.clear();
    out.reserve(m_Meshes.size());
    for (const auto &[id, entry] : m_Meshes) {
        out.push_back(entry);
    }
    return m_Revision;
}

} // namespace Runic::RenderData
