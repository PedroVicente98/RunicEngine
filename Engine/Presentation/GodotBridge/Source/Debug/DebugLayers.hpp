#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <vector>

#include <godot_cpp/classes/camera3d.hpp>
#include <godot_cpp/classes/label3d.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/variant/aabb.hpp>
#include <godot_cpp/variant/rid.hpp>

#include "Debug/DebugMeshCache.hpp"
#include "DebugView.hpp"
#include "Math.hpp"
#include "RenderData/DebugMeshes.hpp"
#include "RenderData/RenderFrame.hpp"

// The debug layers behind the debug view (GDB-2; DebugView.hpp): one RenderingServer mesh and
// instance per DebugBatch, rebuilt from each new RenderFrame with DebugGeometry's streams copied as
// bytes; two unshaded vertex-color materials; a Camera3D set from the frame's camera; a pool of
// Label3D for the text; and, when asked, the development overlay. GDB-4 adds the physics geometry
// layer: the frame's physics geometry in its own two batch meshes, the cached meshes' instances of
// both geometries, and the "see what the server sees" switch, which draws the physics layer with
// the overlay material. Everything runs on Godot's main thread. Private to Runic::GodotBridge.

namespace Runic::GodotBridge::Detail {

class RunicDevelopmentOverlay;

// Adds one surface of DebugGeometry's layout (positions, then colors, as bytes) to a mesh; nothing
// for no vertex. Shared by the frame batches and the cached meshes.
void AddDebugSurface(godot::RID mesh, RenderData::DebugPrimitive primitive,
                     std::span<const Core::Vec3> positions,
                     std::span<const RenderData::PackedColor> colors, const godot::AABB &bounds);

class DebugLayers {
  public:
    DebugLayers() = default;
    ~DebugLayers();
    DebugLayers(const DebugLayers &) = delete;
    DebugLayers &operator=(const DebugLayers &) = delete;

    // After Godot started: adds the nodes and the RenderingServer resources, and checks that
    // Godot's surface layout is DebugGeometry's. meshes: the frame stream's cached meshes (may be
    // nullptr); it outlives the layers.
    void Create(bool developmentOverlay, const RenderData::DebugMeshLibrary *meshes = nullptr);
    // Frees the RenderingServer resources while Godot still runs; the nodes go with its scene tree.
    void Destroy();
    bool IsCreated() const noexcept;

    // Draws the frame's debug geometry and places its camera. shift is where the frame's origin is
    // in Godot's space (the presenter's, GDB-3): the camera, the labels and both batches move by
    // it.
    void Apply(const RenderData::RenderFrame &frame, Core::Vec3 shift);

    void SetLayerVisible(DebugLayer layer, bool visible);
    bool IsLayerVisible(DebugLayer layer) const noexcept;
    const DebugViewStats &Stats() const noexcept;
    bool ReadBack(RenderData::DebugBatch batch, RenderData::DebugPrimitive primitive,
                  std::vector<Core::Vec3> &positions,
                  std::vector<RenderData::PackedColor> &colors) const;
    // The physics layer's surfaces, in Godot's space (the last shift added).
    bool ReadBackPhysics(RenderData::DebugBatch batch, RenderData::DebugPrimitive primitive,
                         std::vector<Core::Vec3> &positions,
                         std::vector<RenderData::PackedColor> &colors) const;
    bool ReadBackMeshInstance(std::size_t index, Core::DebugMeshId &mesh,
                              Core::Vec3 &position) const;
    // Whether the physics layer's depth-tested surfaces use the overlay material now.
    bool PhysicsDrawnOnTop() const noexcept;

  private:
    static constexpr int kNoSurface = -1;
    struct Batch {
        godot::RID mesh;
        godot::RID instance;
        godot::Ref<godot::StandardMaterial3D> material;
        std::array<int, RenderData::kDebugPrimitiveCount> surfaceOf{kNoSurface, kNoSurface};
    };

    using Batches = std::array<Batch, RenderData::kDebugBatchCount>;

    void CreateBatches(Batches &batches, godot::RID scenario);
    void FreeBatches(Batches &batches);
    // Rebuilds one batch's mesh from a geometry; counts what it drew into lines and triangles.
    void Upload(Batch &batch, RenderData::DebugBatch which,
                const RenderData::DebugGeometry &geometry, bool visible,
                const godot::Ref<godot::StandardMaterial3D> &material, std::uint32_t &lines,
                std::uint32_t &triangles);
    // The material the physics layer draws a batch with: the overlay's when the server view is on.
    const godot::Ref<godot::StandardMaterial3D> &
    PhysicsMaterial(RenderData::DebugBatch which) const;
    void ApplyMeshInstances(const RenderData::RenderFrame &frame, godot::Vector3 offset);
    static bool ReadSurface(const Batch &batch, RenderData::DebugPrimitive primitive,
                            Core::Vec3 shift, std::vector<Core::Vec3> &positions,
                            std::vector<RenderData::PackedColor> &colors);
    void ApplyCamera(const RenderData::Camera &camera);
    void ApplyTexts(const RenderData::DebugGeometry &geometry);
    void ApplyVisibility();
    godot::Label3D *NewLabel();

    std::array<bool, kDebugLayerCount> m_Visible{true, true, true, true, false};
    Batches m_Batches;
    Batches m_PhysicsBatches; // GDB-4
    DebugMeshCache m_Meshes;
    const RenderData::DebugMeshLibrary *m_Library = nullptr;
    Core::Vec3 m_Shift{0.0f};            // the last frame's
    godot::Node3D *m_Root = nullptr;     // freed with the scene tree
    godot::Camera3D *m_Camera = nullptr; // a child of m_Root
    std::vector<godot::Label3D *> m_Labels;
    RunicDevelopmentOverlay *m_Overlay = nullptr; // freed with the scene tree
    DebugViewStats m_Stats;
    std::uint64_t m_FramesSinceOverlayRefresh = 0;
    bool m_Created = false;
};

} // namespace Runic::GodotBridge::Detail
