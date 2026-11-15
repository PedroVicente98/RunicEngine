#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include <godot_cpp/classes/camera3d.hpp>
#include <godot_cpp/classes/label3d.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/variant/rid.hpp>

#include "DebugView.hpp"
#include "Math.hpp"
#include "RenderData/RenderFrame.hpp"

// The debug layers behind the debug view (GDB-2; DebugView.hpp): one RenderingServer mesh and
// instance per DebugBatch, rebuilt from each new RenderFrame with DebugGeometry's streams copied as
// bytes; two unshaded vertex-color materials; a Camera3D set from the frame's camera; a pool of
// Label3D for the text; and, when asked, the development overlay. Everything runs on Godot's main
// thread. Private to Runic::GodotBridge.

namespace Runic::GodotBridge::Detail {

class RunicDevelopmentOverlay;

class DebugLayers {
  public:
    DebugLayers() = default;
    ~DebugLayers();
    DebugLayers(const DebugLayers &) = delete;
    DebugLayers &operator=(const DebugLayers &) = delete;

    // After Godot started: adds the nodes and the RenderingServer resources, and checks that
    // Godot's surface layout is DebugGeometry's.
    void Create(bool developmentOverlay);
    // Frees the RenderingServer resources while Godot still runs; the nodes go with its scene tree.
    void Destroy();
    bool IsCreated() const noexcept;

    void Apply(const RenderData::RenderFrame &frame);

    void SetLayerVisible(DebugLayer layer, bool visible);
    bool IsLayerVisible(DebugLayer layer) const noexcept;
    const DebugViewStats &Stats() const noexcept;
    bool ReadBack(RenderData::DebugBatch batch, RenderData::DebugPrimitive primitive,
                  std::vector<Core::Vec3> &positions,
                  std::vector<RenderData::PackedColor> &colors) const;

  private:
    static constexpr int kNoSurface = -1;
    struct Batch {
        godot::RID mesh;
        godot::RID instance;
        godot::Ref<godot::StandardMaterial3D> material;
        std::array<int, RenderData::kDebugPrimitiveCount> surfaceOf{kNoSurface, kNoSurface};
    };

    void Upload(RenderData::DebugBatch which, const RenderData::DebugGeometry &geometry);
    void ApplyCamera(const RenderData::Camera &camera);
    void ApplyTexts(const RenderData::DebugGeometry &geometry);
    void ApplyVisibility();
    godot::Label3D *NewLabel();

    std::array<bool, kDebugLayerCount> m_Visible{true, true, true};
    std::array<Batch, RenderData::kDebugBatchCount> m_Batches;
    godot::Node3D *m_Root = nullptr;     // freed with the scene tree
    godot::Camera3D *m_Camera = nullptr; // a child of m_Root
    std::vector<godot::Label3D *> m_Labels;
    RunicDevelopmentOverlay *m_Overlay = nullptr; // freed with the scene tree
    DebugViewStats m_Stats;
    std::uint64_t m_FramesSinceOverlayRefresh = 0;
    bool m_Created = false;
};

} // namespace Runic::GodotBridge::Detail
