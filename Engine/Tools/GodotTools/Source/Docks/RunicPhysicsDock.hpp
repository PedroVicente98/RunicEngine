#pragma once

#include <chrono>
#include <cstdint>
#include <memory>

#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/check_box.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/tree.hpp>
#include <godot_cpp/classes/v_box_container.hpp>
#include <godot_cpp/variant/packed_vector3_array.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/vector3.hpp>

#include "Docks/ServerViewLayer.hpp"
#include "PhysicsInspector.hpp"
#include "RenderData/DebugGeometry.hpp"
#include "RenderData/DebugMeshes.hpp"
#include "WorldSpace.hpp"

// The physics inspector (GDT-3): a dock over the editor's local simulation's physics world, and
// the "see what the server sees" layer it draws in the editor's viewports. The top shows the
// physics world (bodies, active ones, shapes, characters, steps) and the layer's switches: the
// layer itself, and whether it draws over the visuals (the default) or depth-tested. Then the
// bodies near the editor's camera, and the selected body's properties. A body is selected in the
// list or, with "Pick a body in the viewport" pressed, by the next click in a 3D viewport (the
// plugin forwards it). The selected body's bounds draw over everything. The lists refresh a few
// times a second, the layer when the physics world stepped, the camera moved to another 16 m cell
// or a switch changed; neither changes the world. Private to Runic::GodotTools.

namespace Runic::GodotTools::Detail {

class RunicPhysicsDock : public godot::VBoxContainer {
    GDCLASS(RunicPhysicsDock, godot::VBoxContainer)

  public:
    // The dock's name in the editor, and its node name.
    static constexpr const char *kTitle = "Runic Physics";
    // Its controls' node names.
    static constexpr const char *kServerViewNode = "ServerView";
    static constexpr const char *kOverVisualsNode = "OverVisuals";
    static constexpr const char *kPickNode = "Pick";
    static constexpr const char *kBodiesNode = "Bodies";
    static constexpr const char *kPropertiesNode = "Properties";

    // sceneOrigin: the world position the edited scene's origin stands for.
    void Attach(Simulation::SimulationRuntime &runtime, Core::WorldPosition sceneOrigin);
    void _ready() override;
    void _process(double delta) override;
    void _exit_tree() override;

    // Rebuilds the summary and the lists from the physics world now.
    void Refresh();
    // Redraws the layer when something it shows changed (always when forced).
    void RefreshServerView(bool force);

    bool IsPicking() const noexcept;
    void StopPicking();
    // A ray in the edited scene's space: selects the body it meets, or clears the selection.
    // Returns whether it met one.
    bool PickRay(godot::Vector3 origin, godot::Vector3 direction);

    // Checks, bound for tests through Godot (the dock's class is private): the bodies listed;
    // select the index-th listed body through the list; a property of the selected body as the
    // properties list shows it ("" when none); a layer batch's surface in the scene's space
    // (batch and primitive as RenderData numbers them); whether the physics geometry draws over
    // the visuals; whether the layer is shown.
    int64_t ShownBodies() const noexcept { return m_ShownBodies; }
    bool SelectBody(int64_t index);
    godot::String SelectedProperty(const godot::String &name) const;
    godot::PackedVector3Array ServerViewSurface(int64_t batch, int64_t primitive) const;
    bool ServerViewDrawnOverVisuals() const;
    bool ServerViewShown() const noexcept { return m_ServerViewOn; }

  protected:
    static void _bind_methods();

  private:
    void OnBodySelected();
    void ShowSelected();
    void OnServerViewToggled(bool on);
    void OnOverVisualsToggled(bool on);
    void OnPickToggled(bool on);
    // The world position the layer draws around: the editor camera's, to a 16 m cell.
    Core::WorldPosition ViewOrigin() const;
    void ReportServerView();

    std::unique_ptr<PhysicsInspector> m_Inspector;
    Simulation::SimulationRuntime *m_Runtime = nullptr;
    Core::WorldPosition m_SceneOrigin;
    godot::Label *m_Summary = nullptr;
    godot::CheckBox *m_ServerView = nullptr;
    godot::CheckBox *m_OverVisuals = nullptr;
    godot::Button *m_Pick = nullptr;
    godot::Tree *m_Bodies = nullptr;
    godot::Tree *m_Properties = nullptr;

    ServerViewLayer m_Layer;
    RenderData::DebugGeometry m_Geometry;
    RenderData::DebugMeshLibrary m_Meshes;
    ServerViewSettings m_Settings;
    ServerViewStats m_LastDraw;
    Core::WorldPosition m_DrawnOrigin;
    std::uint64_t m_DrawnSteps = 0;
    Physics::BodyHandle m_DrawnSelection;
    // What the layer's refreshes cost: the geometry drawn and uploaded to Godot.
    std::chrono::steady_clock::duration m_RefreshTime{};
    bool m_ServerViewOn = true;
    bool m_ServerViewDirty = true;

    double m_SinceRefresh = 0.0;
    int64_t m_ShownBodies = 0;
    bool m_ReportedDock = false;
    bool m_ReportedServerView = false;
};

} // namespace Runic::GodotTools::Detail
