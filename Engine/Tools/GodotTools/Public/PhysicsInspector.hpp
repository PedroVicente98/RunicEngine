#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "Math.hpp"
#include "PhysicsWorld.hpp"
#include "RenderData/Color.hpp"
#include "RenderData/DebugGeometry.hpp"
#include "RenderData/DebugMeshes.hpp"
#include "SimulationRuntime.hpp"
#include "WorldSpace.hpp"

// The physics inspector's model (GDT-3): what the Runic Physics dock shows of the local
// simulation's physics world, as plain text, which body is selected, and the geometry of the "see
// what the server sees" layer the editor draws over its viewport. Like WorldInspector it reads the
// simulation between its steps, on the editor's main thread, and never changes it: the selection
// is the inspector's own.

namespace Runic::GodotTools {

// The physics world as a whole.
struct PhysicsSummary {
    // False when the simulation runs no physics (no SimulationRuntime::EnablePhysics): every other
    // member is 0.
    bool enabled = false;
    std::uint32_t bodies = 0;
    std::uint32_t activeBodies = 0;
    std::uint32_t shapes = 0;
    std::uint32_t characters = 0;
    std::uint64_t steps = 0;
    // The bodies and characters entities own (the physics bridge's).
    std::uint32_t entityBodies = 0;
};

struct InspectedProperty {
    std::string name;
    std::string value;
};

// A body as the dock shows it. Its properties, in this order: shape, kind, half extents, motion,
// layer, position, rotation, velocity, friction, restitution, state, sensor, owner (the names are
// the kBody...Property constants).
struct InspectedBody {
    Physics::BodyHandle body;
    std::string label;                         // "body <n>", n its handle
    std::string shape;                         // its shape's name
    std::string motion;                        // "static", "kinematic" or "dynamic"
    std::vector<InspectedProperty> properties; // empty when the body is gone
};

inline constexpr std::string_view kBodyShapeProperty = "shape";
inline constexpr std::string_view kBodyKindProperty = "kind";
inline constexpr std::string_view kBodyHalfExtentsProperty = "half extents";
inline constexpr std::string_view kBodyMotionProperty = "motion";
inline constexpr std::string_view kBodyLayerProperty = "layer";
inline constexpr std::string_view kBodyPositionProperty = "position";
inline constexpr std::string_view kBodyRotationProperty = "rotation";
inline constexpr std::string_view kBodyVelocityProperty = "velocity";
inline constexpr std::string_view kBodyFrictionProperty = "friction";
inline constexpr std::string_view kBodyRestitutionProperty = "restitution";
inline constexpr std::string_view kBodyStateProperty = "state";
inline constexpr std::string_view kBodySensorProperty = "sensor";
inline constexpr std::string_view kBodyOwnerProperty = "owner";

// The most bodies a list holds: a dock shows a page.
inline constexpr std::uint32_t kDefaultBodyListLimit = 500;
// The dock lists, and the layer draws, what comes within this distance of the editor's camera: a
// little more than what a server streams around a player at the start.
inline constexpr double kDefaultInspectRadiusM = 250.0;
// The farthest a click in the viewport picks.
inline constexpr double kDefaultPickDistanceM = 1'000.0;
// The selected body: its bounds, a little larger than its shape, over everything.
inline constexpr RenderData::PackedColor kSelectedBodyColor = RenderData::kDebugQuery;
inline constexpr float kSelectionPaddingM = 0.02f;

// What the "see what the server sees" layer draws.
struct ServerViewSettings {
    double radiusM = kDefaultInspectRadiusM;
    // Over the visuals (the overlay batch, no depth test): collision shows through what hides it,
    // so a rock that looks solid without collision shows up. Off: depth-tested, as the visuals.
    bool overVisuals = true;
    bool faces = false;      // translucent faces under the wireframe
    bool velocities = false; // an arrow for each moving body
};

struct ServerViewStats {
    Physics::PhysicsDebugStats physics; // available is false when there is no physics to draw
    bool selectionDrawn = false;
};

class PhysicsInspector {
  public:
    // runtime outlives the inspector.
    explicit PhysicsInspector(Simulation::SimulationRuntime &runtime) noexcept
        : m_Runtime(runtime) {}

    PhysicsSummary Summary() const;
    // The bodies whose bounds come within radiusM of center, in handle order, at most limit of
    // them; none without physics.
    std::vector<Physics::BodyHandle> Bodies(Core::WorldPosition center,
                                            double radiusM = kDefaultInspectRadiusM,
                                            std::uint32_t limit = kDefaultBodyListLimit) const;
    // A body's label, shape, motion and properties; empty properties when it is gone.
    InspectedBody Inspect(Physics::BodyHandle body) const;

    // The nearest body a ray meets within maxDistanceM: boxes and spheres by their exact shape,
    // capsules by their box. Meshes (terrain) are picked from the list, not by a ray, until
    // Physics has ray queries (PHYS-5): a mesh's bounds cover what stands on it. None when the ray
    // meets nothing (or there is no physics).
    Physics::BodyHandle Pick(Core::WorldPosition origin, Core::Vec3 direction,
                             double maxDistanceM = kDefaultPickDistanceM) const;

    // A none handle clears the selection.
    void Select(Physics::BodyHandle body) noexcept { m_Selected = body; }
    // The selected body; none when nothing is, or when it is gone.
    Physics::BodyHandle Selected() const;

    // The layer's geometry around origin (the frame's origin: positions are relative to it),
    // appended to out: the physics world's debug view (PhysicsWorld::ExtractDebugGeometry, with
    // meshes cached in the library when one is given) and the selected body's bounds in the
    // overlay batch. Nothing without physics.
    ServerViewStats DrawServerView(Core::WorldPosition origin, const ServerViewSettings &settings,
                                   RenderData::DebugGeometry &out,
                                   RenderData::DebugMeshLibrary *meshes = nullptr);

  private:
    Simulation::SimulationRuntime &m_Runtime;
    Physics::BodyHandle m_Selected;
};

} // namespace Runic::GodotTools
