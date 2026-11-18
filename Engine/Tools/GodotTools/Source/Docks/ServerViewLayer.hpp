#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <vector>

#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/variant/rid.hpp>
#include <godot_cpp/variant/vector3.hpp>

#include "Math.hpp"
#include "PlainIds.hpp"
#include "RenderData/DebugGeometry.hpp"
#include "RenderData/DebugMeshes.hpp"

// The "see what the server sees" layer on Godot's side (GDT-3): draws the geometry a
// PhysicsInspector writes (DrawServerView) into the world the editor's 3D viewports render, the
// one the edited scene and the editor's own gizmos live in, so the collision of the local
// simulation stands over the visuals it should match. The depth-tested batch draws with a
// depth-tested material; the overlay batch with no depth test and the highest priority, over every
// visual. Large shapes come as cached meshes: one RenderingServer mesh per mesh of the layer's
// library, uploaded when it appears or changes, and pooled instances that each refresh places.
// Godot's main thread only. GodotBridge's debug layers (GDB-4) draw the same data for the client,
// but they are private to GodotBridge and make their own camera current, which the editor's root
// window must not have: ARCHITECTURE.md records the interface change that would share them.
// Private to Runic::GodotTools.

namespace Runic::GodotTools::Detail {

// What the layer drew at its last refresh.
struct ServerViewDrawn {
    std::uint32_t lineVertices = 0;
    std::uint32_t triangleVertices = 0;
    std::uint32_t meshInstances = 0;
    std::uint32_t cachedMeshes = 0;
    std::uint32_t droppedVertices = 0; // the geometry was full: what it could not hold
    std::uint64_t meshUploads = 0;     // since the layer was created
    std::uint64_t refreshes = 0;       // since the layer was created
};

class ServerViewLayer {
  public:
    ServerViewLayer() = default;
    ~ServerViewLayer();
    ServerViewLayer(const ServerViewLayer &) = delete;
    ServerViewLayer &operator=(const ServerViewLayer &) = delete;

    // Creates the layer in a scenario. False, and nothing drawn later, when Godot no longer lays
    // a surface out as DebugGeometry does.
    bool Create(godot::RID scenario);
    void Destroy();
    bool IsCreated() const noexcept { return m_Created; }

    // Draws geometry, its positions relative to translation (the geometry's origin in the scene),
    // and the library's meshes its instances name.
    void Show(const RenderData::DebugGeometry &geometry, const RenderData::DebugMeshLibrary &meshes,
              godot::Vector3 translation);
    void SetVisible(bool visible);
    bool IsVisible() const noexcept { return m_Visible; }
    const ServerViewDrawn &Drawn() const noexcept { return m_Drawn; }

    // Checks: a batch's surface as Godot holds it, in the scene's space; whether a batch's surfaces
    // draw with no depth test.
    std::vector<Core::Vec3> ReadSurface(RenderData::DebugBatch batch,
                                        RenderData::DebugPrimitive primitive) const;
    bool DrawsOverEverything(RenderData::DebugBatch batch) const;

  private:
    static constexpr std::size_t kBatches = 2;
    static constexpr std::size_t kPrimitives = 2;
    static constexpr int kNoSurface = -1;

    struct Batch {
        godot::RID mesh;
        godot::RID instance;
        godot::Ref<godot::StandardMaterial3D> material;
        std::array<int, kPrimitives> surfaceOf{kNoSurface, kNoSurface};
    };
    struct Cached {
        godot::RID mesh;
        std::uint64_t revision = 0;
    };
    struct Placed {
        godot::RID instance;
        Core::DebugMeshId mesh;
        godot::RID material;
    };

    void Upload(Batch &batch, const RenderData::DebugGeometry &geometry,
                RenderData::DebugBatch which);
    void SyncMeshes(const RenderData::DebugMeshLibrary &library);
    void PlaceMeshes(const RenderData::DebugGeometry &geometry, godot::Vector3 translation);

    std::array<Batch, kBatches> m_Batches;
    std::map<Core::DebugMeshId, Cached> m_Meshes;
    std::vector<Placed> m_Pool;
    std::vector<RenderData::DebugMeshLibrary::Entry> m_Snapshot;
    std::uint64_t m_SyncedRevision = 0;
    std::size_t m_Placed = 0;
    godot::RID m_Scenario;
    godot::Vector3 m_Translation;
    ServerViewDrawn m_Drawn;
    bool m_LayoutMatches = false;
    bool m_Visible = true;
    bool m_Created = false;
};

} // namespace Runic::GodotTools::Detail
