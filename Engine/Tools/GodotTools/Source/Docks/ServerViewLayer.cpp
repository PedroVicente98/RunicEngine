// The "see what the server sees" layer on Godot's side (ServerViewLayer.hpp).

#include "Docks/ServerViewLayer.hpp"

#include <cstring>
#include <limits>
#include <span>

#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/variant/aabb.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/basis.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/packed_vector3_array.hpp>
#include <godot_cpp/variant/quaternion.hpp>
#include <godot_cpp/variant/transform3d.hpp>

#include "Log.hpp"
#include "WorldSpace.hpp"

namespace Runic::GodotTools::Detail {

namespace {

using godot::RenderingServer;
using RenderData::DebugBatch;
using RenderData::DebugPrimitive;

// Positions and colors only, uncompressed, at Godot's current surface format: 3 floats per vertex
// in the vertex stream, the bytes R, G, B, A per vertex in the attribute stream.
constexpr std::uint64_t kSurfaceFormat = RenderingServer::ARRAY_FORMAT_VERTEX |
                                         RenderingServer::ARRAY_FORMAT_COLOR |
                                         RenderingServer::ARRAY_FLAG_FORMAT_CURRENT_VERSION;
constexpr uint32_t kStreamStart = 0; // positions and colors each start their stream
// The keys of RenderingServer::mesh_add_surface's dictionary.
constexpr const char *kPrimitiveKey = "primitive";
constexpr const char *kFormatKey = "format";
constexpr const char *kVertexDataKey = "vertex_data";
constexpr const char *kAttributeDataKey = "attribute_data";
constexpr const char *kVertexCountKey = "vertex_count";
constexpr const char *kAabbKey = "aabb";

// The layer is never culled: its bounds are everything a frame can hold (ToLocal's range).
constexpr float kBoundsHalfExtentM = static_cast<float>(Core::kMaxLocalOffsetM);
const godot::AABB kLayerBounds(godot::Vector3(-kBoundsHalfExtentM, -kBoundsHalfExtentM,
                                              -kBoundsHalfExtentM),
                               godot::Vector3(kBoundsHalfExtentM + kBoundsHalfExtentM,
                                              kBoundsHalfExtentM + kBoundsHalfExtentM,
                                              kBoundsHalfExtentM + kBoundsHalfExtentM));

constexpr std::size_t IndexOf(DebugBatch batch) noexcept { return static_cast<std::size_t>(batch); }
constexpr std::size_t IndexOf(DebugPrimitive primitive) noexcept {
    return static_cast<std::size_t>(primitive);
}

godot::PackedByteArray BytesOf(const void *data, std::size_t size) {
    godot::PackedByteArray bytes;
    bytes.resize(static_cast<int64_t>(size));
    std::memcpy(bytes.ptrw(), data, size);
    return bytes;
}

// Whether Godot stores a vertex-and-color surface exactly as DebugGeometry lays it out.
bool LayoutMatches(RenderingServer &server) {
    const godot::BitField<RenderingServer::ArrayFormat> format(
        static_cast<int64_t>(kSurfaceFormat));
    const auto vertices = static_cast<int32_t>(RenderData::kVerticesPerLine);
    const uint32_t vertexStride = server.mesh_surface_get_format_vertex_stride(format, vertices);
    const uint32_t attributeStride =
        server.mesh_surface_get_format_attribute_stride(format, vertices);
    const bool matches =
        vertexStride == sizeof(Core::Vec3) && attributeStride == sizeof(RenderData::PackedColor) &&
        server.mesh_surface_get_format_offset(format, vertices, RenderingServer::ARRAY_VERTEX) ==
            kStreamStart &&
        server.mesh_surface_get_format_offset(format, vertices, RenderingServer::ARRAY_COLOR) ==
            kStreamStart;
    if (!matches) {
        RUNIC_LOG_WARNING("RUNIC: server view: Godot's vertex stride is {} and its attribute "
                          "stride {}, DebugGeometry's {} and {}: the layer draws nothing",
                          vertexStride, attributeStride, sizeof(Core::Vec3),
                          sizeof(RenderData::PackedColor));
    }
    return matches;
}

// Returns whether a surface was added (a stream with vertices).
bool AddSurface(godot::RID mesh, DebugPrimitive primitive, std::span<const Core::Vec3> positions,
                std::span<const RenderData::PackedColor> colors, const godot::AABB &bounds) {
    if (positions.empty()) {
        return false;
    }
    godot::Dictionary data;
    data[kPrimitiveKey] = static_cast<int64_t>(primitive == DebugPrimitive::Lines
                                                   ? RenderingServer::PRIMITIVE_LINES
                                                   : RenderingServer::PRIMITIVE_TRIANGLES);
    data[kFormatKey] = static_cast<int64_t>(kSurfaceFormat);
    data[kVertexDataKey] = BytesOf(positions.data(), positions.size_bytes());
    data[kAttributeDataKey] = BytesOf(colors.data(), colors.size_bytes());
    data[kVertexCountKey] = static_cast<int64_t>(positions.size());
    data[kAabbKey] = bounds;
    RenderingServer::get_singleton()->mesh_add_surface(mesh, data);
    return true;
}

// Unshaded, colored by its vertices (sRGB bytes), translucent by their alpha, two-sided; the
// overlay batch also skips the depth test and draws after everything else.
godot::Ref<godot::StandardMaterial3D> MakeMaterial(DebugBatch batch) {
    godot::Ref<godot::StandardMaterial3D> material;
    material.instantiate();
    material->set_shading_mode(godot::BaseMaterial3D::SHADING_MODE_UNSHADED);
    material->set_flag(godot::BaseMaterial3D::FLAG_ALBEDO_FROM_VERTEX_COLOR, true);
    material->set_flag(godot::BaseMaterial3D::FLAG_SRGB_VERTEX_COLOR, true);
    material->set_flag(godot::BaseMaterial3D::FLAG_DISABLE_FOG, true);
    material->set_transparency(godot::BaseMaterial3D::TRANSPARENCY_ALPHA);
    material->set_cull_mode(godot::BaseMaterial3D::CULL_DISABLED);
    if (batch == DebugBatch::Overlay) {
        material->set_flag(godot::BaseMaterial3D::FLAG_DISABLE_DEPTH_TEST, true);
        material->set_render_priority(godot::Material::RENDER_PRIORITY_MAX);
    }
    return material;
}

// A cached mesh's bounds, so Godot culls its instances like any mesh.
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

ServerViewLayer::~ServerViewLayer() { Destroy(); }

bool ServerViewLayer::Create(godot::RID scenario) {
    RenderingServer *server = RenderingServer::get_singleton();
    if (m_Created || server == nullptr) {
        return m_Created;
    }
    m_LayoutMatches = LayoutMatches(*server);
    m_Scenario = scenario;
    for (const DebugBatch which : {DebugBatch::WorldDepth, DebugBatch::Overlay}) {
        Batch &batch = m_Batches[IndexOf(which)];
        batch.mesh = server->mesh_create();
        batch.instance = server->instance_create2(batch.mesh, scenario);
        server->instance_set_custom_aabb(batch.instance, kLayerBounds);
        server->instance_geometry_set_cast_shadows_setting(
            batch.instance, RenderingServer::SHADOW_CASTING_SETTING_OFF);
        batch.material = MakeMaterial(which);
    }
    m_Created = true;
    SetVisible(m_Visible);
    return m_LayoutMatches;
}

void ServerViewLayer::Destroy() {
    if (!m_Created) {
        return;
    }
    if (RenderingServer *server = RenderingServer::get_singleton()) {
        for (Batch &batch : m_Batches) {
            server->free_rid(batch.instance);
            server->free_rid(batch.mesh);
        }
        for (const Placed &placed : m_Pool) {
            server->free_rid(placed.instance);
        }
        for (const auto &[id, cached] : m_Meshes) {
            server->free_rid(cached.mesh);
        }
    }
    for (Batch &batch : m_Batches) {
        batch.material.unref();
        batch.surfaceOf.fill(kNoSurface);
    }
    m_Pool.clear();
    m_Meshes.clear();
    m_Placed = 0;
    m_SyncedRevision = 0;
    m_Created = false;
}

void ServerViewLayer::Show(const RenderData::DebugGeometry &geometry,
                           const RenderData::DebugMeshLibrary &meshes, godot::Vector3 translation) {
    if (!m_Created) {
        return;
    }
    RenderingServer *server = RenderingServer::get_singleton();
    m_Translation = translation;
    m_Drawn.lineVertices = 0;
    m_Drawn.triangleVertices = 0;
    m_Drawn.droppedVertices = geometry.DroppedVertices();
    for (const DebugBatch which : {DebugBatch::WorldDepth, DebugBatch::Overlay}) {
        Batch &batch = m_Batches[IndexOf(which)];
        server->instance_set_transform(batch.instance,
                                       godot::Transform3D(godot::Basis(), translation));
        Upload(batch, geometry, which);
    }
    SyncMeshes(meshes);
    PlaceMeshes(geometry, translation);
    ++m_Drawn.refreshes;
}

void ServerViewLayer::Upload(Batch &batch, const RenderData::DebugGeometry &geometry,
                             DebugBatch which) {
    RenderingServer *server = RenderingServer::get_singleton();
    server->mesh_clear(batch.mesh);
    batch.surfaceOf.fill(kNoSurface);
    if (!m_LayoutMatches) {
        return;
    }
    int surface = 0;
    for (const DebugPrimitive primitive : {DebugPrimitive::Lines, DebugPrimitive::Triangles}) {
        const auto positions = geometry.Positions(which, primitive);
        if (!AddSurface(batch.mesh, primitive, positions, geometry.Colors(which, primitive),
                        kLayerBounds)) {
            continue;
        }
        server->mesh_surface_set_material(batch.mesh, surface, batch.material->get_rid());
        batch.surfaceOf[IndexOf(primitive)] = surface++;
        const auto vertices = static_cast<std::uint32_t>(positions.size());
        (primitive == DebugPrimitive::Lines ? m_Drawn.lineVertices : m_Drawn.triangleVertices) +=
            vertices;
    }
}

void ServerViewLayer::SyncMeshes(const RenderData::DebugMeshLibrary &library) {
    if (!m_LayoutMatches || library.Revision() == m_SyncedRevision) {
        return;
    }
    m_SyncedRevision = library.Snapshot(m_Snapshot);
    RenderingServer *server = RenderingServer::get_singleton();
    for (const RenderData::DebugMeshLibrary::Entry &entry : m_Snapshot) {
        auto cached = m_Meshes.find(entry.id);
        if (cached == m_Meshes.end()) {
            cached = m_Meshes.emplace(entry.id, Cached{server->mesh_create(), 0}).first;
        }
        if (cached->second.revision != entry.revision) {
            // Cleared and filled again in place: the instances that draw it keep their base.
            server->mesh_clear(cached->second.mesh);
            const godot::AABB bounds = BoundsOf(*entry.mesh);
            AddSurface(cached->second.mesh, DebugPrimitive::Lines, entry.mesh->linePositions,
                       entry.mesh->lineColors, bounds);
            AddSurface(cached->second.mesh, DebugPrimitive::Triangles,
                       entry.mesh->trianglePositions, entry.mesh->triangleColors, bounds);
            cached->second.revision = entry.revision;
            ++m_Drawn.meshUploads;
        }
    }
    // What the library no longer has (both lists are in ID order).
    auto listed = m_Snapshot.begin();
    for (auto cached = m_Meshes.begin(); cached != m_Meshes.end();) {
        while (listed != m_Snapshot.end() && listed->id < cached->first) {
            ++listed;
        }
        if (listed != m_Snapshot.end() && listed->id == cached->first) {
            ++cached;
            continue;
        }
        for (Placed &placed : m_Pool) {
            if (placed.mesh == cached->first) {
                server->instance_set_base(placed.instance, godot::RID());
                server->instance_set_visible(placed.instance, false);
                placed.mesh = Core::DebugMeshId();
            }
        }
        server->free_rid(cached->second.mesh);
        cached = m_Meshes.erase(cached);
    }
    m_Drawn.cachedMeshes = static_cast<std::uint32_t>(m_Meshes.size());
}

void ServerViewLayer::PlaceMeshes(const RenderData::DebugGeometry &geometry,
                                  godot::Vector3 translation) {
    RenderingServer *server = RenderingServer::get_singleton();
    m_Placed = 0;
    for (const RenderData::DebugMeshInstance &instance : geometry.MeshInstances()) {
        const auto cached = m_Meshes.find(instance.mesh);
        if (cached == m_Meshes.end()) {
            continue; // not synced: the library did not hold it
        }
        if (m_Placed == m_Pool.size()) {
            Placed placed;
            placed.instance = server->instance_create2(cached->second.mesh, m_Scenario);
            placed.mesh = instance.mesh;
            server->instance_geometry_set_cast_shadows_setting(
                placed.instance, RenderingServer::SHADOW_CASTING_SETTING_OFF);
            m_Pool.push_back(placed);
        }
        Placed &placed = m_Pool[m_Placed++];
        if (placed.mesh != instance.mesh) {
            server->instance_set_base(placed.instance, cached->second.mesh);
            placed.mesh = instance.mesh;
        }
        const godot::RID material = m_Batches[IndexOf(instance.batch)].material->get_rid();
        if (placed.material != material) {
            server->instance_geometry_set_material_override(placed.instance, material);
            placed.material = material;
        }
        const godot::Quaternion rotation(instance.rotation.x, instance.rotation.y,
                                         instance.rotation.z, instance.rotation.w);
        const godot::Vector3 at =
            godot::Vector3(instance.position.x, instance.position.y, instance.position.z) +
            translation;
        server->instance_set_transform(placed.instance,
                                       godot::Transform3D(godot::Basis(rotation), at));
        server->instance_set_visible(placed.instance, m_Visible);
    }
    for (std::size_t i = m_Placed; i < m_Pool.size(); ++i) {
        server->instance_set_visible(m_Pool[i].instance, false);
    }
    m_Drawn.meshInstances = static_cast<std::uint32_t>(m_Placed);
}

void ServerViewLayer::SetVisible(bool visible) {
    m_Visible = visible;
    if (!m_Created) {
        return;
    }
    RenderingServer *server = RenderingServer::get_singleton();
    for (const Batch &batch : m_Batches) {
        server->instance_set_visible(batch.instance, visible);
    }
    for (std::size_t i = 0; i < m_Pool.size(); ++i) {
        server->instance_set_visible(m_Pool[i].instance, visible && i < m_Placed);
    }
}

std::vector<Core::Vec3> ServerViewLayer::ReadSurface(DebugBatch which,
                                                     DebugPrimitive primitive) const {
    std::vector<Core::Vec3> positions;
    const Batch &batch = m_Batches[IndexOf(which)];
    const int surface = batch.surfaceOf[IndexOf(primitive)];
    if (!m_Created || surface == kNoSurface) {
        return positions;
    }
    const godot::Array arrays =
        RenderingServer::get_singleton()->mesh_surface_get_arrays(batch.mesh, surface);
    const godot::PackedVector3Array vertices = arrays[RenderingServer::ARRAY_VERTEX];
    positions.reserve(static_cast<std::size_t>(vertices.size()));
    for (int64_t i = 0; i < vertices.size(); ++i) {
        const godot::Vector3 v = vertices[i] + m_Translation;
        positions.emplace_back(v.x, v.y, v.z);
    }
    return positions;
}

bool ServerViewLayer::DrawsOverEverything(DebugBatch which) const {
    const Batch &batch = m_Batches[IndexOf(which)];
    if (!m_Created || !batch.material->get_flag(godot::BaseMaterial3D::FLAG_DISABLE_DEPTH_TEST)) {
        return false;
    }
    RenderingServer *server = RenderingServer::get_singleton();
    for (const int surface : batch.surfaceOf) {
        if (surface != kNoSurface &&
            server->mesh_surface_get_material(batch.mesh, surface) != batch.material->get_rid()) {
            return false;
        }
    }
    return true;
}

} // namespace Runic::GodotTools::Detail
