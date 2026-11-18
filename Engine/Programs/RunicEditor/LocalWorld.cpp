// The RunicEditor's local world (LocalWorld.hpp).

#include "LocalWorld.hpp"

#include <cmath>
#include <format>
#include <string>

#include <glm/ext/scalar_constants.hpp>

#include "EntityWorld.hpp"
#include "PhysicsBridge.hpp"
#include "WorldComponents.hpp"

namespace Runic::Programs {

namespace {

constexpr std::uint32_t kMarkers = 12;
constexpr double kRingRadiusM = 20.0;
constexpr double kTwoPi = glm::two_pi<double>();
// Each marker drifts around the ring at its own pace.
constexpr float kSlowestDriftMps = 0.5f;
constexpr float kDriftStepMps = 0.25f;
constexpr Core::WorldId kLocalWorldId{1};
const Core::WorldPosition kRegionOrigin{0.0, 0.0, 0.0};

// The physics the docks inspect (GDT-3): a 60 m static floor whose top is the region's ground, the
// markers as 1 m kinematic boxes standing on it (the bridge moves them with their entities), and
// two 1 m crates stacked at the centre, which settle and sleep.
const Core::Vec3 kFloorHalfExtentsM(30.0f, 0.5f, 30.0f);
const Core::Vec3 kBoxHalfExtentsM(0.5f);
constexpr double kFloorCentreY = -0.5;
constexpr double kStandingY = 0.5; // a 1 m box on the floor
constexpr double kStackStepY = 1.0;
constexpr std::uint32_t kCrates = 2;
constexpr std::string_view kFloorShapeName = "engine:editor_floor";
constexpr std::string_view kBoxShapeName = "engine:editor_box";

constexpr std::string_view kDriftName = "engine:editor_drift";
constexpr std::string_view kDriftDescription =
    "How fast a marker of the editor's local world drifts";
constexpr std::string_view kDriftQueryName = "engine:editor_drift";

struct Drift {
    float speedMps = 0.0f;
};

void FormatDrift(const void *component, std::string &out) {
    out = std::format("{:.2f} m/s", static_cast<const Drift *>(component)->speedMps);
}

} // namespace

void PopulateLocalWorld(Simulation::SimulationRuntime &runtime) {
    // Before the scene's own systems, so the bridge's run first in its phases.
    Simulation::PhysicsBridge &physics = runtime.EnablePhysics();
    Physics::PhysicsWorld &bodies = physics.World();
    const Physics::ShapeId floorShape =
        bodies.AddShape(kFloorShapeName, Physics::BoxShape(kFloorHalfExtentsM)).Value();
    const Physics::ShapeId boxShape =
        bodies.AddShape(kBoxShapeName, Physics::BoxShape(kBoxHalfExtentsM)).Value();
    const auto bodyOf = [&](Physics::ShapeId shape, Physics::MotionType motion) {
        Physics::BodyDesc desc;
        desc.shape = shape;
        desc.motion = motion;
        desc.layer =
            bodies.Layers().Find(motion == Physics::MotionType::Static ? Physics::kStaticLayerName
                                                                       : Physics::kMovingLayerName);
        return desc;
    };

    ECS::EntityWorld &world = runtime.Entities();
    ECS::ComponentOptions options;
    options.description = kDriftDescription;
    options.format = &FormatDrift;
    world.RegisterComponent<Drift>(kDriftName, options);

    const ECS::Entity region = world.Create();
    world.Set(region, World::Region{kLocalWorldId, World::RegionFrame{kRegionOrigin}});
    for (std::uint32_t i = 0; i < kMarkers; ++i) {
        const double angle = kTwoPi * static_cast<double>(i) / kMarkers;
        const ECS::Entity marker = world.Create();
        world.Set(marker, World::WorldTransform{{kRingRadiusM * std::cos(angle), kStandingY,
                                                 kRingRadiusM * std::sin(angle)},
                                                World::kNoRotation});
        world.Set(marker, Drift{kSlowestDriftMps + kDriftStepMps * static_cast<float>(i)});
        physics.AddBody(marker, bodyOf(boxShape, Physics::MotionType::Kinematic));
    }
    const ECS::Entity floor = world.Create();
    world.Set(floor, World::WorldTransform{
                         {kRegionOrigin.x, kRegionOrigin.y + kFloorCentreY, kRegionOrigin.z},
                         World::kNoRotation});
    physics.AddBody(floor, bodyOf(floorShape, Physics::MotionType::Static));
    for (std::uint32_t i = 0; i < kCrates; ++i) {
        const ECS::Entity crate = world.Create();
        world.Set(crate, World::WorldTransform{{kRegionOrigin.x,
                                                kRegionOrigin.y + kStandingY + kStackStepY * i,
                                                kRegionOrigin.z},
                                               World::kNoRotation});
        physics.AddBody(crate, bodyOf(boxShape, Physics::MotionType::Dynamic));
    }

    const auto drifting = world.MakeQuery<World::WorldTransform, const Drift>(kDriftQueryName);
    runtime.AddSystem(
        {"engine:editor_drift", Simulation::ScheduleSlot::Hz30Realtime,
         [drifting](const Simulation::TickContext &context) {
             drifting.Each([&](ECS::Entity, World::WorldTransform &transform, const Drift &drift) {
                 // Along the ring: a step perpendicular to the way to its center.
                 const Core::WorldPosition at = transform.position;
                 const double radius = std::hypot(at.x, at.z);
                 const double step = drift.speedMps * context.dt;
                 transform.position.x -= at.z / radius * step;
                 transform.position.z += at.x / radius * step;
             });
         },
         Simulation::RealtimePhase::Movement});
}

} // namespace Runic::Programs
