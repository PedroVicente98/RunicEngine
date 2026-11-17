#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <vector>

#include <godot_cpp/variant/rid.hpp>
#include <godot_cpp/variant/vector3.hpp>

#include "Math.hpp"
#include "PlainIds.hpp"
#include "RenderData/DebugGeometry.hpp"
#include "RenderData/DebugMeshes.hpp"

// Cached debug meshes on Godot (GDB-4): one RenderingServer mesh per mesh of the frame stream's
// DebugMeshLibrary, uploaded when it appears or changes and freed when it goes, and a pool of
// RenderingServer instances that each frame places at its mesh instances. A frame that lists the
// same meshes costs a transform per instance, never a vertex. Godot's main thread only. Private to
// Runic::GodotBridge.

namespace Runic::GodotBridge::Detail {

class DebugMeshCache {
  public:
    DebugMeshCache() = default;
    ~DebugMeshCache();
    DebugMeshCache(const DebugMeshCache &) = delete;
    DebugMeshCache &operator=(const DebugMeshCache &) = delete;

    void Create(godot::RID scenario);
    void Destroy();

    // Uploads the meshes the library added or replaced since the last sync and frees the ones it
    // removed; nothing when its revision did not move.
    void Sync(const RenderData::DebugMeshLibrary &library);

    // A frame's instances: Begin, Place each, End (which hides the pool's unused instances).
    void BeginFrame() noexcept;
    // A mesh the cache does not hold (not synced yet) is skipped.
    void Place(const RenderData::DebugMeshInstance &instance, godot::Vector3 shift,
               godot::RID material);
    void EndFrame();

    std::uint32_t Meshes() const noexcept { return static_cast<std::uint32_t>(m_Meshes.size()); }
    std::uint32_t Placed() const noexcept { return static_cast<std::uint32_t>(m_Placed); }
    std::uint64_t Uploads() const noexcept { return m_Uploads; }
    // The index-th instance placed in the last frame: its mesh and its place in Godot's space.
    bool ReadBack(std::size_t index, Core::DebugMeshId &mesh, Core::Vec3 &position) const;

  private:
    struct Held {
        godot::RID mesh;
        std::uint64_t revision = 0;
    };
    struct Drawn {
        godot::RID instance;
        Core::DebugMeshId mesh;
        godot::RID material;
        Core::Vec3 position{0.0f};
    };

    void Upload(godot::RID mesh, const RenderData::DebugMesh &geometry);
    void Forget(Core::DebugMeshId id);

    std::map<Core::DebugMeshId, Held> m_Meshes;
    std::vector<Drawn> m_Pool;
    std::vector<RenderData::DebugMeshLibrary::Entry> m_Snapshot;
    std::size_t m_Placed = 0;
    std::uint64_t m_Uploads = 0;
    std::uint64_t m_SyncedRevision = 0;
    godot::RID m_Scenario;
    bool m_Created = false;
};

} // namespace Runic::GodotBridge::Detail
