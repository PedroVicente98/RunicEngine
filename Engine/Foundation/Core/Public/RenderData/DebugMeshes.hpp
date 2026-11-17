#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <vector>

#include "Math.hpp"
#include "PlainIds.hpp"
#include "RenderData/Color.hpp"

// Cached debug meshes (RunicDocs › Render data › Debug geometry: lines, triangles, text, cached
// debug meshes; GDB-4's "cached debug meshes for large shapes"). Geometry too big to rewrite every
// frame, such as a terrain's collision mesh, is put once into a DebugMeshLibrary and drawn by
// instances that each frame lists in its DebugGeometry (DebugMeshInstance): the reader uploads a
// mesh when it appears or changes and moves instances, so a frame carries a few dozen bytes per
// mesh, not its vertices.

namespace Runic::RenderData {

// A mesh in its own space, metres: line vertex pairs and triangle vertex triples with their colors,
// the same two streams a DebugGeometry batch has. Immutable once in a library.
struct DebugMesh {
    std::vector<Core::Vec3> linePositions;
    std::vector<PackedColor> lineColors;
    std::vector<Core::Vec3> trianglePositions;
    std::vector<PackedColor> triangleColors;

    std::size_t VertexCount() const noexcept {
        return linePositions.size() + trianglePositions.size();
    }
};

// The meshes a frame stream's instances refer to, shared by its writer and its reader (the frame
// exchange owns one). Put and Remove are rare (a shape seen for the first time, a cell unloaded)
// and take a lock; the reader compares Revision with the one it synced and walks the library only
// when it changed. A mesh is shared, never copied: a reader may keep drawing one that was replaced
// until it syncs.
class DebugMeshLibrary {
  public:
    // A mesh as the reader syncs it: the revision of the library when it was put.
    struct Entry {
        Core::DebugMeshId id;
        std::uint64_t revision = 0;
        std::shared_ptr<const DebugMesh> mesh;
    };

    // Adds a mesh under its ID, or replaces the one there.
    void Put(Core::DebugMeshId id, std::shared_ptr<const DebugMesh> mesh);
    // Removes a mesh; an ID the library does not hold is ignored.
    void Remove(Core::DebugMeshId id);
    bool Contains(Core::DebugMeshId id) const;
    std::size_t Count() const;
    // Changes with every Put and Remove: 0 for a library never changed.
    std::uint64_t Revision() const;
    // Every mesh, by ID, and the library's revision at the time.
    std::uint64_t Snapshot(std::vector<Entry> &out) const;

  private:
    mutable std::mutex m_Mutex;
    std::map<Core::DebugMeshId, Entry> m_Meshes;
    std::uint64_t m_Revision = 0;
};

} // namespace Runic::RenderData
