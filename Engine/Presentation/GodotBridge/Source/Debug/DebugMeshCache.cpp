// Cached debug meshes on Godot (DebugMeshCache.hpp).

#include "Debug/DebugMeshCache.hpp"

#include "Debug/DebugLayers.hpp"

#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/variant/aabb.hpp>
#include <godot_cpp/variant/basis.hpp>
#include <godot_cpp/variant/quaternion.hpp>
#include <godot_cpp/variant/transform3d.hpp>

#include <algorithm>
#include <limits>

namespace Runic::GodotBridge::Detail {

namespace {

using godot::RenderingServer;

// A mesh's bounds, so Godot culls its instances like any mesh.
godot::AABB BoundsOf(const RenderData::DebugMesh &mesh) {
    Core::Vec3 low(std::numeric_limits<float>::max());
    Core::Vec3 high(std::numeric_limits<float>::lowest());
    for (const auto *points : {&mesh.linePositions, &mesh.trianglePositions}) {
        for (const Core::Vec3 &p : *points) {
            low = glm::min(low, p);
            high = glm::max(high, p);
        }
    }
    if (low.x > high.x) {
        return godot::AABB();
    }
    return godot::AABB(godot::Vector3(low.x, low.y, low.z),
                       godot::Vector3(high.x - low.x, high.y - low.y, high.z - low.z));
}

} // namespace

DebugMeshCache::~DebugMeshCache() { Destroy(); }

void DebugMeshCache::Create(godot::RID scenario) {
    m_Scenario = scenario;
    m_Created = true;
}

void DebugMeshCache::Destroy() {
    if (!m_Created) {
        return;
    }
    if (RenderingServer *server = RenderingServer::get_singleton()) {
        for (const Drawn &drawn : m_Pool) {
            server->free_rid(drawn.instance);
        }
        for (const auto &[id, held] : m_Meshes) {
            server->free_rid(held.mesh);
        }
    }
    m_Pool.clear();
    m_Meshes.clear();
    m_Placed = 0;
    m_SyncedRevision = 0;
    m_Created = false;
}

void DebugMeshCache::Sync(const RenderData::DebugMeshLibrary &library) {
    if (!m_Created || library.Revision() == m_SyncedRevision) {
        return;
    }
    m_SyncedRevision = library.Snapshot(m_Snapshot);
    RenderingServer *server = RenderingServer::get_singleton();
    for (const RenderData::DebugMeshLibrary::Entry &entry : m_Snapshot) {
        auto held = m_Meshes.find(entry.id);
        if (held == m_Meshes.end()) {
            held = m_Meshes.emplace(entry.id, Held{server->mesh_create(), 0}).first;
        }
        if (held->second.revision != entry.revision) {
            // Cleared and filled again in place: the instances that draw it keep their base.
            server->mesh_clear(held->second.mesh);
            Upload(held->second.mesh, *entry.mesh);
            held->second.revision = entry.revision;
            ++m_Uploads;
        }
    }
    // What the library no longer has (both lists are in ID order).
    std::vector<Core::DebugMeshId> gone;
    auto listed = m_Snapshot.begin();
    for (const auto &[id, held] : m_Meshes) {
        while (listed != m_Snapshot.end() && listed->id < id) {
            ++listed;
        }
        if (listed == m_Snapshot.end() || listed->id != id) {
            gone.push_back(id);
        }
    }
    for (const Core::DebugMeshId id : gone) {
        Forget(id);
    }
}

void DebugMeshCache::Upload(godot::RID mesh, const RenderData::DebugMesh &geometry) {
    const godot::AABB bounds = BoundsOf(geometry);
    AddDebugSurface(mesh, RenderData::DebugPrimitive::Lines, geometry.linePositions,
                    geometry.lineColors, bounds);
    AddDebugSurface(mesh, RenderData::DebugPrimitive::Triangles, geometry.trianglePositions,
                    geometry.triangleColors, bounds);
}

void DebugMeshCache::Forget(Core::DebugMeshId id) {
    const auto held = m_Meshes.find(id);
    if (held == m_Meshes.end()) {
        return;
    }
    RenderingServer *server = RenderingServer::get_singleton();
    for (Drawn &drawn : m_Pool) {
        if (drawn.mesh == id) {
            server->instance_set_base(drawn.instance, godot::RID());
            server->instance_set_visible(drawn.instance, false);
            drawn.mesh = Core::DebugMeshId();
        }
    }
    server->free_rid(held->second.mesh);
    m_Meshes.erase(held);
}

void DebugMeshCache::BeginFrame() noexcept { m_Placed = 0; }

void DebugMeshCache::Place(const RenderData::DebugMeshInstance &instance, godot::Vector3 shift,
                           godot::RID material) {
    const auto held = m_Meshes.find(instance.mesh);
    if (!m_Created || held == m_Meshes.end()) {
        return;
    }
    RenderingServer *server = RenderingServer::get_singleton();
    if (m_Placed == m_Pool.size()) {
        Drawn drawn;
        drawn.instance = server->instance_create2(held->second.mesh, m_Scenario);
        drawn.mesh = instance.mesh;
        server->instance_geometry_set_cast_shadows_setting(
            drawn.instance, RenderingServer::SHADOW_CASTING_SETTING_OFF);
        m_Pool.push_back(drawn);
    }
    Drawn &drawn = m_Pool[m_Placed++];
    if (drawn.mesh != instance.mesh) {
        server->instance_set_base(drawn.instance, held->second.mesh);
        drawn.mesh = instance.mesh;
    }
    if (drawn.material != material) {
        server->instance_geometry_set_material_override(drawn.instance, material);
        drawn.material = material;
    }
    const godot::Quaternion rotation(instance.rotation.x, instance.rotation.y, instance.rotation.z,
                                     instance.rotation.w);
    const godot::Vector3 at =
        godot::Vector3(instance.position.x, instance.position.y, instance.position.z) + shift;
    server->instance_set_transform(drawn.instance, godot::Transform3D(godot::Basis(rotation), at));
    server->instance_set_visible(drawn.instance, true);
    drawn.position = Core::Vec3(at.x, at.y, at.z);
}

void DebugMeshCache::EndFrame() {
    RenderingServer *server = RenderingServer::get_singleton();
    for (std::size_t i = m_Placed; i < m_Pool.size(); ++i) {
        server->instance_set_visible(m_Pool[i].instance, false);
    }
}

bool DebugMeshCache::ReadBack(std::size_t index, Core::DebugMeshId &mesh,
                              Core::Vec3 &position) const {
    if (index >= m_Placed) {
        return false;
    }
    mesh = m_Pool[index].mesh;
    position = m_Pool[index].position;
    return true;
}

} // namespace Runic::GodotBridge::Detail
