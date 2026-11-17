// The debug layers (DebugLayers.hpp): DebugGeometry to the RenderingServer with no per-vertex work,
// the frame's camera, the labels and the overlay (RunicDocs › Debug drawing (Engine) › Drawing
// DebugGeometry with Godot).

#include "Debug/DebugLayers.hpp"

#include "Debug/DevelopmentOverlay.hpp"
#include "GodotLayer/SceneRoot.hpp"
#include "Log.hpp"
#include "WorldSpace.hpp"

#include <godot_cpp/classes/base_material3d.hpp>
#include <godot_cpp/classes/material.hpp>
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/classes/window.hpp>
#include <godot_cpp/classes/world3d.hpp>
#include <godot_cpp/variant/aabb.hpp>
#include <godot_cpp/variant/basis.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/packed_color_array.hpp>
#include <godot_cpp/variant/packed_vector3_array.hpp>
#include <godot_cpp/variant/quaternion.hpp>
#include <godot_cpp/variant/transform3d.hpp>
#include <godot_cpp/variant/vector3.hpp>

#include <cmath>
#include <cstddef>
#include <cstring>
#include <limits>
#include <string_view>

namespace Runic::GodotBridge {

namespace {

constexpr std::string_view kDepthShapesLabel = "Depth-tested shapes";
constexpr std::string_view kOverlayShapesLabel = "Always-on-top shapes";
constexpr std::string_view kTextLabel = "Text";
constexpr std::string_view kPhysicsGeometryLabel = "Physics geometry";
constexpr std::string_view kServerViewLabel = "See what the server sees";
constexpr std::string_view kDepthShapesToggle = "RunicToggleDepthShapes";
constexpr std::string_view kOverlayShapesToggle = "RunicToggleOverlayShapes";
constexpr std::string_view kTextToggle = "RunicToggleText";
constexpr std::string_view kPhysicsGeometryToggle = "RunicTogglePhysicsGeometry";
constexpr std::string_view kServerViewToggle = "RunicToggleServerView";

} // namespace

std::string_view DebugLayerLabel(DebugLayer layer) noexcept {
    switch (layer) {
    case DebugLayer::DepthShapes:
        return kDepthShapesLabel;
    case DebugLayer::OverlayShapes:
        return kOverlayShapesLabel;
    case DebugLayer::Text:
        return kTextLabel;
    case DebugLayer::PhysicsGeometry:
        return kPhysicsGeometryLabel;
    case DebugLayer::ServerView:
        return kServerViewLabel;
    }
    return kTextLabel;
}

std::string_view DebugLayerToggleName(DebugLayer layer) noexcept {
    switch (layer) {
    case DebugLayer::DepthShapes:
        return kDepthShapesToggle;
    case DebugLayer::OverlayShapes:
        return kOverlayShapesToggle;
    case DebugLayer::Text:
        return kTextToggle;
    case DebugLayer::PhysicsGeometry:
        return kPhysicsGeometryToggle;
    case DebugLayer::ServerView:
        return kServerViewToggle;
    }
    return kTextToggle;
}

namespace Detail {

namespace {

using godot::RenderingServer;
using RenderData::DebugBatch;
using RenderData::DebugPrimitive;

// Positions and colors only, uncompressed, at Godot's current surface format: 3 floats per vertex
// in the vertex stream, the bytes R, G, B, A per vertex in the attribute stream.
constexpr std::uint64_t kSurfaceFormat = RenderingServer::ARRAY_FORMAT_VERTEX |
                                         RenderingServer::ARRAY_FORMAT_COLOR |
                                         RenderingServer::ARRAY_FLAG_FORMAT_CURRENT_VERSION;
// Positions and colors each start their stream: nothing comes before them.
constexpr uint32_t kStreamStart = 0;
// The keys of RenderingServer::mesh_add_surface's dictionary.
constexpr const char *kPrimitiveKey = "primitive";
constexpr const char *kFormatKey = "format";
constexpr const char *kVertexDataKey = "vertex_data";
constexpr const char *kAttributeDataKey = "attribute_data";
constexpr const char *kVertexCountKey = "vertex_count";
constexpr const char *kAabbKey = "aabb";

// Debug geometry is never culled: its bounds are everything a frame can hold (ToLocal's range).
constexpr float kBoundsHalfExtentM = static_cast<float>(Core::kMaxLocalOffsetM);
const godot::AABB kDebugBounds(godot::Vector3(-kBoundsHalfExtentM, -kBoundsHalfExtentM,
                                              -kBoundsHalfExtentM),
                               godot::Vector3(kBoundsHalfExtentM + kBoundsHalfExtentM,
                                              kBoundsHalfExtentM + kBoundsHalfExtentM,
                                              kBoundsHalfExtentM + kBoundsHalfExtentM));

// Labels: billboard text of a fixed size on screen, with a dark outline so it reads on any
// background.
constexpr int kLabelFontSize = 32;
constexpr float kLabelPixelSize = 0.0015f;
constexpr int kLabelOutlineSize = 8;
const godot::Color kLabelOutlineColor(0.0f, 0.0f, 0.0f);
constexpr float kChannelMax = std::numeric_limits<std::uint8_t>::max();
// The labels shown while the Text layer is hidden.
constexpr std::size_t kNoLabels = 0;
// The overlay's stats line is rewritten every this many applied frames.
constexpr std::uint64_t kOverlayRefreshFrames = 15;

constexpr std::size_t IndexOf(DebugBatch batch) noexcept { return static_cast<std::size_t>(batch); }
constexpr std::size_t IndexOf(DebugPrimitive primitive) noexcept {
    return static_cast<std::size_t>(primitive);
}
constexpr std::size_t IndexOf(DebugLayer layer) noexcept { return static_cast<std::size_t>(layer); }
constexpr DebugLayer LayerOf(DebugBatch batch) noexcept {
    return batch == DebugBatch::WorldDepth ? DebugLayer::DepthShapes : DebugLayer::OverlayShapes;
}

godot::PackedByteArray BytesOf(const void *data, std::size_t size) {
    godot::PackedByteArray bytes;
    bytes.resize(static_cast<int64_t>(size));
    std::memcpy(bytes.ptrw(), data, size);
    return bytes;
}

godot::Color ToColor(RenderData::PackedColor color) {
    return godot::Color(
        RenderData::RedOf(color) / kChannelMax, RenderData::GreenOf(color) / kChannelMax,
        RenderData::BlueOf(color) / kChannelMax, RenderData::AlphaOf(color) / kChannelMax);
}

std::uint8_t ToChannel(float channel) {
    return static_cast<std::uint8_t>(std::lround(channel * kChannelMax));
}

godot::String ToGodot(std::string_view text) {
    return godot::String::utf8(text.data(), static_cast<int64_t>(text.size()));
}

godot::Ref<godot::StandardMaterial3D> MakeMaterial(DebugBatch batch) {
    godot::Ref<godot::StandardMaterial3D> material;
    material.instantiate();
    material->set_shading_mode(godot::BaseMaterial3D::SHADING_MODE_UNSHADED);
    material->set_flag(godot::BaseMaterial3D::FLAG_ALBEDO_FROM_VERTEX_COLOR, true);
    material->set_flag(godot::BaseMaterial3D::FLAG_SRGB_VERTEX_COLOR, true);
    material->set_flag(godot::BaseMaterial3D::FLAG_DISABLE_FOG, true);
    material->set_transparency(godot::BaseMaterial3D::TRANSPARENCY_ALPHA); // opacity is in alpha
    material->set_cull_mode(godot::BaseMaterial3D::CULL_DISABLED);
    if (batch == DebugBatch::Overlay) {
        material->set_flag(godot::BaseMaterial3D::FLAG_DISABLE_DEPTH_TEST, true);
        material->set_render_priority(godot::Material::RENDER_PRIORITY_MAX);
    }
    return material;
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
        RUNIC_LOG_WARNING(
            "RUNIC: debug view: Godot's vertex stride is {} and its attribute stride {}, "
            "DebugGeometry's {} and {}: debug geometry is not drawn",
            vertexStride, attributeStride, sizeof(Core::Vec3), sizeof(RenderData::PackedColor));
    }
    return matches;
}

} // namespace

void AddDebugSurface(godot::RID mesh, DebugPrimitive primitive,
                     std::span<const Core::Vec3> positions,
                     std::span<const RenderData::PackedColor> colors, const godot::AABB &bounds) {
    if (positions.empty()) {
        return;
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
}

DebugLayers::~DebugLayers() { Destroy(); }

void DebugLayers::CreateBatches(Batches &batches, godot::RID scenario) {
    RenderingServer *server = RenderingServer::get_singleton();
    for (const DebugBatch which : {DebugBatch::WorldDepth, DebugBatch::Overlay}) {
        Batch &batch = batches[IndexOf(which)];
        batch.mesh = server->mesh_create();
        batch.instance = server->instance_create2(batch.mesh, scenario);
        server->instance_set_custom_aabb(batch.instance, kDebugBounds);
        server->instance_geometry_set_cast_shadows_setting(
            batch.instance, RenderingServer::SHADOW_CASTING_SETTING_OFF);
        batch.material = MakeMaterial(which);
    }
}

void DebugLayers::FreeBatches(Batches &batches) {
    RenderingServer *server = RenderingServer::get_singleton();
    for (Batch &batch : batches) {
        if (server != nullptr) {
            server->free_rid(batch.instance);
            server->free_rid(batch.mesh);
        }
        batch.material.unref();
        batch.surfaceOf.fill(kNoSurface);
    }
}

void DebugLayers::Create(bool developmentOverlay, const RenderData::DebugMeshLibrary *meshes) {
    godot::Window *root = SceneRoot();
    RenderingServer *server = RenderingServer::get_singleton();
    if (m_Created || root == nullptr || server == nullptr) {
        return;
    }
    m_Stats.layoutMatches = LayoutMatches(*server);

    m_Root = memnew(godot::Node3D);
    m_Root->set_name(ToGodot(kDebugViewNodeName));
    root->add_child(m_Root);
    m_Camera = memnew(godot::Camera3D);
    m_Camera->set_name(ToGodot(kDebugCameraNodeName));
    m_Root->add_child(m_Camera);
    m_Camera->make_current();

    const godot::RID scenario = root->get_world_3d()->get_scenario();
    CreateBatches(m_Batches, scenario);
    CreateBatches(m_PhysicsBatches, scenario);
    m_Meshes.Create(scenario);
    m_Library = meshes;
    if (developmentOverlay) {
        m_Overlay = memnew(RunicDevelopmentOverlay);
        m_Overlay->set_name(ToGodot(kDevelopmentOverlayNodeName));
        m_Overlay->Build(*this);
        root->add_child(m_Overlay);
    }
    m_Created = true;
    ApplyVisibility();
}

void DebugLayers::Destroy() {
    if (!m_Created) {
        return;
    }
    RUNIC_LOG_INFO("RUNIC: debug view stopped at frame {}: {} line vertices, {} triangle vertices, "
                   "{} texts in {} surfaces",
                   m_Stats.frame, m_Stats.lineVertices, m_Stats.triangleVertices, m_Stats.texts,
                   m_Stats.surfaces);
    RUNIC_LOG_INFO("RUNIC: physics geometry stopped at frame {}: {} line vertices, {} triangle "
                   "vertices, {} mesh instances of {} cached meshes ({} uploads)",
                   m_Stats.frame, m_Stats.physicsLineVertices, m_Stats.physicsTriangleVertices,
                   m_Stats.meshInstances, m_Stats.cachedMeshes, m_Stats.meshUploads);
    if (m_Overlay != nullptr) {
        m_Overlay->Detach();
    }
    FreeBatches(m_Batches);
    FreeBatches(m_PhysicsBatches);
    m_Meshes.Destroy();
    m_Library = nullptr;
    m_Root = nullptr;
    m_Camera = nullptr;
    m_Overlay = nullptr;
    m_Labels.clear();
    m_Created = false;
}

bool DebugLayers::IsCreated() const noexcept { return m_Created; }

void DebugLayers::Apply(const RenderData::RenderFrame &frame, Core::Vec3 shift) {
    if (!m_Created) {
        return;
    }
    // Everything below is relative to the frame's origin: the root node carries the camera and the
    // labels there, and each batch's instance carries its mesh. No vertex is touched.
    const godot::Vector3 offset(shift.x, shift.y, shift.z);
    m_Shift = shift;
    m_Root->set_position(offset);
    RenderingServer *server = RenderingServer::get_singleton();
    for (const Batches *batches : {&m_Batches, &m_PhysicsBatches}) {
        for (const Batch &batch : *batches) {
            server->instance_set_transform(batch.instance,
                                           godot::Transform3D(godot::Basis(), offset));
        }
    }
    m_Stats.frame = frame.number;
    m_Stats.lineVertices = 0;
    m_Stats.triangleVertices = 0;
    m_Stats.physicsLineVertices = 0;
    m_Stats.physicsTriangleVertices = 0;
    m_Stats.surfaces = 0;
    m_Stats.droppedVertices = frame.debug.DroppedVertices() + frame.physics.DroppedVertices();
    m_Stats.droppedTexts = frame.debug.DroppedTexts();
    ApplyCamera(frame.camera);
    const bool physics = IsLayerVisible(DebugLayer::PhysicsGeometry);
    for (const DebugBatch which : {DebugBatch::WorldDepth, DebugBatch::Overlay}) {
        Upload(m_Batches[IndexOf(which)], which, frame.debug, IsLayerVisible(LayerOf(which)),
               m_Batches[IndexOf(which)].material, m_Stats.lineVertices, m_Stats.triangleVertices);
        Upload(m_PhysicsBatches[IndexOf(which)], which, frame.physics, physics,
               PhysicsMaterial(which), m_Stats.physicsLineVertices,
               m_Stats.physicsTriangleVertices);
    }
    ApplyMeshInstances(frame, offset);
    ApplyTexts(frame.debug);
    if (m_Overlay != nullptr && ++m_FramesSinceOverlayRefresh >= kOverlayRefreshFrames) {
        m_FramesSinceOverlayRefresh = 0;
        m_Overlay->ShowStats(m_Stats);
    }
}

void DebugLayers::Upload(Batch &batch, DebugBatch which, const RenderData::DebugGeometry &geometry,
                         bool visible, const godot::Ref<godot::StandardMaterial3D> &material,
                         std::uint32_t &lines, std::uint32_t &triangles) {
    RenderingServer *server = RenderingServer::get_singleton();
    server->mesh_clear(batch.mesh);
    batch.surfaceOf.fill(kNoSurface);
    if (!m_Stats.layoutMatches || !visible) {
        return;
    }
    int surface = 0;
    for (const DebugPrimitive primitive : {DebugPrimitive::Lines, DebugPrimitive::Triangles}) {
        const auto positions = geometry.Positions(which, primitive);
        if (positions.empty()) {
            continue;
        }
        AddDebugSurface(batch.mesh, primitive, positions, geometry.Colors(which, primitive),
                        kDebugBounds);
        server->mesh_surface_set_material(batch.mesh, surface, material->get_rid());
        batch.surfaceOf[IndexOf(primitive)] = surface++;
        ++m_Stats.surfaces;
        const auto vertices = static_cast<std::uint32_t>(positions.size());
        (primitive == DebugPrimitive::Lines ? lines : triangles) += vertices;
    }
}

const godot::Ref<godot::StandardMaterial3D> &DebugLayers::PhysicsMaterial(DebugBatch which) const {
    // "See what the server sees": the physics layer draws over everything, as the overlay does.
    const DebugBatch drawnAs = IsLayerVisible(DebugLayer::ServerView) ? DebugBatch::Overlay : which;
    return m_PhysicsBatches[IndexOf(drawnAs)].material;
}

void DebugLayers::ApplyMeshInstances(const RenderData::RenderFrame &frame, godot::Vector3 offset) {
    if (m_Library != nullptr) {
        m_Meshes.Sync(*m_Library);
    }
    m_Meshes.BeginFrame();
    if (m_Stats.layoutMatches) {
        for (const RenderData::DebugMeshInstance &instance : frame.debug.MeshInstances()) {
            if (IsLayerVisible(LayerOf(instance.batch))) {
                m_Meshes.Place(instance, offset,
                               m_Batches[IndexOf(instance.batch)].material->get_rid());
            }
        }
        if (IsLayerVisible(DebugLayer::PhysicsGeometry)) {
            for (const RenderData::DebugMeshInstance &instance : frame.physics.MeshInstances()) {
                m_Meshes.Place(instance, offset, PhysicsMaterial(instance.batch)->get_rid());
            }
        }
    }
    m_Meshes.EndFrame();
    m_Stats.meshInstances = m_Meshes.Placed();
    m_Stats.cachedMeshes = m_Meshes.Meshes();
    m_Stats.meshUploads = m_Meshes.Uploads();
}

void DebugLayers::ApplyCamera(const RenderData::Camera &camera) {
    const godot::Quaternion rotation(camera.rotation.x, camera.rotation.y, camera.rotation.z,
                                     camera.rotation.w);
    m_Camera->set_transform(godot::Transform3D(
        godot::Basis(rotation),
        godot::Vector3(camera.position.x, camera.position.y, camera.position.z)));
    m_Camera->set_fov(camera.verticalFovRadians / RenderData::kRadiansPerDegree);
    m_Camera->set_near(camera.nearPlaneM);
    m_Camera->set_far(camera.farPlaneM);
}

void DebugLayers::ApplyTexts(const RenderData::DebugGeometry &geometry) {
    const auto texts = geometry.Texts();
    const std::size_t shown = IsLayerVisible(DebugLayer::Text) ? texts.size() : kNoLabels;
    while (m_Labels.size() < shown) {
        m_Labels.push_back(NewLabel());
    }
    for (std::size_t i = 0; i < m_Labels.size(); ++i) {
        godot::Label3D *label = m_Labels[i];
        if (i >= shown) {
            label->set_visible(false);
            continue;
        }
        const RenderData::DebugText &text = texts[i];
        label->set_text(ToGodot(geometry.TextOf(text)));
        label->set_position(godot::Vector3(text.position.x, text.position.y, text.position.z));
        label->set_modulate(ToColor(text.color));
        label->set_draw_flag(godot::Label3D::FLAG_DISABLE_DEPTH_TEST,
                             text.batch == DebugBatch::Overlay);
        label->set_visible(true);
    }
    m_Stats.texts = static_cast<std::uint32_t>(shown);
}

godot::Label3D *DebugLayers::NewLabel() {
    auto *label = memnew(godot::Label3D);
    label->set_billboard_mode(godot::BaseMaterial3D::BILLBOARD_ENABLED);
    label->set_draw_flag(godot::Label3D::FLAG_FIXED_SIZE, true);
    label->set_draw_flag(godot::Label3D::FLAG_SHADED, false);
    label->set_font_size(kLabelFontSize);
    label->set_pixel_size(kLabelPixelSize);
    label->set_outline_size(kLabelOutlineSize);
    label->set_outline_modulate(kLabelOutlineColor);
    m_Root->add_child(label);
    return label;
}

void DebugLayers::SetLayerVisible(DebugLayer layer, bool visible) {
    m_Visible[IndexOf(layer)] = visible;
    if (m_Created) {
        ApplyVisibility();
    }
}

void DebugLayers::ApplyVisibility() {
    RenderingServer *server = RenderingServer::get_singleton();
    for (const DebugBatch which : {DebugBatch::WorldDepth, DebugBatch::Overlay}) {
        server->instance_set_visible(m_Batches[IndexOf(which)].instance,
                                     IsLayerVisible(LayerOf(which)));
        server->instance_set_visible(m_PhysicsBatches[IndexOf(which)].instance,
                                     IsLayerVisible(DebugLayer::PhysicsGeometry));
    }
    if (!IsLayerVisible(DebugLayer::Text)) {
        for (godot::Label3D *label : m_Labels) {
            label->set_visible(false);
        }
    }
    if (m_Overlay != nullptr) {
        for (const DebugLayer layer :
             {DebugLayer::DepthShapes, DebugLayer::OverlayShapes, DebugLayer::Text,
              DebugLayer::PhysicsGeometry, DebugLayer::ServerView}) {
            m_Overlay->FollowLayer(layer, IsLayerVisible(layer));
        }
    }
}

bool DebugLayers::IsLayerVisible(DebugLayer layer) const noexcept {
    return m_Visible[IndexOf(layer)];
}

const DebugViewStats &DebugLayers::Stats() const noexcept { return m_Stats; }

bool DebugLayers::ReadSurface(const Batch &batch, DebugPrimitive primitive, Core::Vec3 shift,
                              std::vector<Core::Vec3> &positions,
                              std::vector<RenderData::PackedColor> &colors) {
    positions.clear();
    colors.clear();
    const int surface = batch.surfaceOf[IndexOf(primitive)];
    if (surface == kNoSurface) {
        return false;
    }
    const godot::Array arrays =
        RenderingServer::get_singleton()->mesh_surface_get_arrays(batch.mesh, surface);
    const godot::PackedVector3Array vertices = arrays[RenderingServer::ARRAY_VERTEX];
    const godot::PackedColorArray vertexColors = arrays[RenderingServer::ARRAY_COLOR];
    for (int64_t i = 0; i < vertices.size(); ++i) {
        positions.push_back(Core::Vec3(vertices[i].x, vertices[i].y, vertices[i].z) + shift);
    }
    for (int64_t i = 0; i < vertexColors.size(); ++i) {
        const godot::Color &color = vertexColors[i];
        colors.push_back(RenderData::PackABGR(ToChannel(color.r), ToChannel(color.g),
                                              ToChannel(color.b), ToChannel(color.a)));
    }
    return true;
}

bool DebugLayers::ReadBack(DebugBatch batch, DebugPrimitive primitive,
                           std::vector<Core::Vec3> &positions,
                           std::vector<RenderData::PackedColor> &colors) const {
    positions.clear();
    colors.clear();
    return m_Created &&
           ReadSurface(m_Batches[IndexOf(batch)], primitive, Core::Vec3(0.0f), positions, colors);
}

bool DebugLayers::ReadBackPhysics(DebugBatch batch, DebugPrimitive primitive,
                                  std::vector<Core::Vec3> &positions,
                                  std::vector<RenderData::PackedColor> &colors) const {
    positions.clear();
    colors.clear();
    return m_Created &&
           ReadSurface(m_PhysicsBatches[IndexOf(batch)], primitive, m_Shift, positions, colors);
}

bool DebugLayers::ReadBackMeshInstance(std::size_t index, Core::DebugMeshId &mesh,
                                       Core::Vec3 &position) const {
    return m_Created && m_Meshes.ReadBack(index, mesh, position);
}

bool DebugLayers::PhysicsDrawnOnTop() const noexcept {
    if (!m_Created) {
        return false;
    }
    // What Godot holds: the material of the depth batch's first surface, when it has one.
    const Batch &depth = m_PhysicsBatches[IndexOf(DebugBatch::WorldDepth)];
    const godot::Ref<godot::StandardMaterial3D> &overlay =
        m_PhysicsBatches[IndexOf(DebugBatch::Overlay)].material;
    for (const int surface : depth.surfaceOf) {
        if (surface != kNoSurface) {
            return RenderingServer::get_singleton()->mesh_surface_get_material(
                       depth.mesh, surface) == overlay->get_rid();
        }
    }
    return IsLayerVisible(DebugLayer::ServerView);
}

} // namespace Detail

} // namespace Runic::GodotBridge
