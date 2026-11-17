#pragma once

#include <array>
#include <cstdint>
#include <unordered_map>
#include <vector>

#include <godot_cpp/classes/capsule_mesh.hpp>
#include <godot_cpp/classes/primitive_mesh.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/variant/rid.hpp>

#include "GodotPresenter.hpp"
#include "PlainIds.hpp"
#include "RenderData/RenderFrame.hpp"

// The presenter behind GodotPresenter.hpp (GDB-3): one RenderingServer instance per shape
// instance, found by its RenderInstanceId; unit meshes per shape kind (a mesh per size for
// capsules, whose ends stay round); one material per color; and the presenter's own origin.
// Everything runs on Godot's main thread. Private to Runic::GodotBridge.

namespace Runic::GodotBridge::Detail {

class Presenter {
  public:
    Presenter() = default;
    ~Presenter();
    Presenter(const Presenter &) = delete;
    Presenter &operator=(const Presenter &) = delete;

    // After Godot started: makes the unit meshes in the root's 3D world.
    void Create();
    // Frees every instance, mesh and material while Godot still runs.
    void Destroy();
    bool IsCreated() const noexcept { return m_Created; }

    // Applies a frame's instances, moving the presenter's origin first when the frame's is too
    // far from it. Returns where the frame's origin is in Godot's space: what everything the frame
    // places relative to its origin (the camera, the debug geometry) must be moved by.
    Core::Vec3 Apply(const RenderData::RenderFrame &frame);

    const PresenterStats &Stats() const noexcept { return m_Stats; }
    bool ReadBack(Core::RenderInstanceId id, PresentedInstance &out) const;

  private:
    static constexpr std::uint32_t kNoSlot = UINT32_MAX;

    struct Slot {
        Core::RenderInstanceId id;
        godot::RID instance;
        godot::RID mesh;
        RenderData::ShapeKind shape = RenderData::ShapeKind::Box;
        RenderData::PackedColor color = RenderData::kDebugInactive;
        Core::Vec3 size{0.0f};
        Core::WorldPosition world; // where the shape is
        Core::Quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
        Core::Vec3 position{0.0f}; // where Godot has it: world relative to the origin
        Core::Vec3 scale{1.0f};
        std::uint64_t seen = 0; // the last pass that matched it by ID
        bool live = false;
    };

    void Rebase(Core::WorldPosition origin);
    void ApplyChanged(const RenderData::ShapeInstanceList &shapes, Core::WorldPosition origin);
    void Reconcile(const RenderData::ShapeInstanceList &shapes, Core::WorldPosition origin);
    std::uint32_t NewSlot(const RenderData::ShapeInstance &shape, Core::WorldPosition origin);
    // Brings a slot to what the frame says. exact: compare places exactly (the writer says the
    // shape changed); otherwise a place within kSamePlaceM is the same place.
    void Write(Slot &slot, const RenderData::ShapeInstance &shape, Core::WorldPosition origin,
               bool exact);
    void Place(Slot &slot);
    godot::RID MeshFor(RenderData::ShapeKind shape, Core::Vec3 size, Core::Vec3 &scale);
    godot::RID MaterialFor(RenderData::PackedColor color);

    std::vector<Slot> m_Slots;
    std::vector<std::uint32_t> m_FreeSlots;
    std::unordered_map<std::uint64_t, std::uint32_t> m_SlotOfId;
    std::vector<std::uint32_t> m_SlotOfIndex; // the applied list's index → its slot

    std::array<godot::Ref<godot::PrimitiveMesh>, RenderData::kShapeKindCount> m_UnitMeshes;
    std::unordered_map<std::uint64_t, godot::Ref<godot::CapsuleMesh>> m_Capsules;
    std::unordered_map<RenderData::PackedColor, godot::Ref<godot::StandardMaterial3D>> m_Materials;
    godot::RID m_Scenario;

    Core::WorldPosition m_Origin;
    bool m_HasOrigin = false;
    std::uint64_t m_AppliedRevision = 0;
    bool m_HasApplied = false;
    std::uint64_t m_Pass = 0;
    PresenterStats m_Stats;
    bool m_Created = false;
};

} // namespace Runic::GodotBridge::Detail
