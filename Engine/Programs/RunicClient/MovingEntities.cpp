// RunicClient's moving entities (MovingEntities.hpp).

#include "MovingEntities.hpp"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <glm/ext/quaternion_trigonometric.hpp>
#include <glm/ext/scalar_constants.hpp>

#include "CommandQueue.hpp"
#include "DebugShapes.hpp"
#include "EntityWorld.hpp"
#include "Log.hpp"
#include "Time.hpp"
#include "WorldComponents.hpp"

namespace Runic::Programs {

namespace {

using Core::Vec3;
using Core::WorldPosition;

constexpr float kTwoPi = glm::two_pi<float>();
const Vec3 kUp(0.0f, 1.0f, 0.0f);

// The camera circles the plane once a minute, looking at its center from above.
constexpr double kCameraOrbitPeriodS = 60.0;
constexpr double kCameraOrbitRadiusM = 30.0;
constexpr double kCameraHeightM = 18.0;
const WorldPosition kPlaneCenter{0.0, 0.0, 0.0};

// Sixteen walkers on a ring 14 m from the center, each patrolling a square 4 m on a side.
constexpr std::uint32_t kWalkers = 16;
constexpr double kRingRadiusM = 14.0;
constexpr double kPatrolHalfSideM = 2.0;
constexpr std::size_t kCorners = 4;
constexpr std::array<double, kCorners> kCornerX{-1.0, 1.0, 1.0, -1.0};
constexpr std::array<double, kCorners> kCornerZ{-1.0, -1.0, 1.0, 1.0};
// Speeds from 1.5 to 3 m/s, so the walkers drift apart.
constexpr float kSlowestSpeedMps = 1.5f;
constexpr float kSpeedStepMps = 0.1f;
// A walker within this distance of its corner has reached it.
constexpr double kArrivedM = 0.1;

// Four still crates at the plane's corners.
constexpr std::uint32_t kCrates = 4;
constexpr double kCrateDistanceM = 19.0;
const Vec3 kCrateSize(1.0f, 1.0f, 1.0f);
const Vec3 kCrateOffset(0.0f, 0.5f, 0.0f);

// A walker is a capsule the size of a person, standing on the plane.
const Vec3 kWalkerSize(0.8f, 1.8f, 0.8f);
const Vec3 kWalkerOffset(0.0f, 0.9f, 0.0f);

// Render IDs: walkers from 1, crates after them.
constexpr std::uint64_t kFirstWalkerId = 1;
constexpr std::uint64_t kFirstCrateId = kFirstWalkerId + kWalkers;

constexpr std::string_view kVelocityName = "engine:demo_velocity";
constexpr std::string_view kSteerQueueName = "engine:demo_steer";
constexpr std::string_view kMovingQueryName = "engine:demo_moving";

// How a walker moves: set only by Steer commands.
struct DemoVelocity {
    Vec3 value{0.0f};
};

// The one command: head that way at that speed.
struct Steer {
    ECS::Entity walker;
    Vec3 velocity{0.0f};
};

struct Patrol {
    ECS::Entity walker;
    WorldPosition center;
    std::size_t corner = 0;
    float speed = 0.0f;
};

WorldPosition CornerOf(const Patrol &patrol) {
    return {patrol.center.x + kCornerX[patrol.corner] * kPatrolHalfSideM, patrol.center.y,
            patrol.center.z + kCornerZ[patrol.corner] * kPatrolHalfSideM};
}

double FlatDistance(WorldPosition a, WorldPosition b) { return std::hypot(a.x - b.x, a.z - b.z); }

// Toward the patrol's corner at its speed, from where the walker is.
Steer SteerToCorner(const Patrol &patrol, WorldPosition from) {
    const WorldPosition corner = CornerOf(patrol);
    const double distance = FlatDistance(corner, from);
    const Vec3 direction(static_cast<float>((corner.x - from.x) / distance), 0.0f,
                         static_cast<float>((corner.z - from.z) / distance));
    return {patrol.walker, direction * patrol.speed};
}

} // namespace

struct MovingEntitiesLayer::State {
    State(Simulation::SimulationRuntime &simulation, Client::ClientCamera &view)
        : runtime(simulation), camera(view), steering(simulation, kSteerQueueName) {}

    Simulation::SimulationRuntime &runtime;
    Client::ClientCamera &camera;
    Simulation::CommandQueue<Steer> steering;
    std::vector<Patrol> patrols;
    double walkedM = 0.0;
};

MovingEntitiesLayer::MovingEntitiesLayer(Simulation::SimulationRuntime &runtime,
                                         Client::ClientCamera &camera)
    : Layer(std::string(kMovingEntitiesLayerName)),
      m_State(std::make_unique<State>(runtime, camera)) {
    State &state = *m_State;
    ECS::EntityWorld &world = runtime.Entities();
    Client::RegisterClientComponents(world);
    world.RegisterComponent<DemoVelocity>(kVelocityName);

    for (std::uint32_t i = 0; i < kWalkers; ++i) {
        const double angle = kTwoPi * static_cast<double>(i) / kWalkers;
        Patrol patrol;
        patrol.center = {kRingRadiusM * std::cos(angle), 0.0, kRingRadiusM * std::sin(angle)};
        patrol.speed = kSlowestSpeedMps + kSpeedStepMps * static_cast<float>(i);
        patrol.walker = world.Create();
        const WorldPosition start = CornerOf(patrol); // at its first corner, heading for the next
        patrol.corner = (patrol.corner + 1) % kCorners;
        world.Set(patrol.walker, World::WorldTransform{start, World::kNoRotation});
        world.Set(patrol.walker, DemoVelocity{});
        world.Set(patrol.walker, Client::DebugShape{RenderData::ShapeKind::Capsule, kWalkerSize,
                                                    kWalkerOffset, RenderData::kDebugPredicted});
        world.Set(patrol.walker,
                  Client::RenderIdentity{Core::RenderInstanceId(kFirstWalkerId + i)});
        state.steering.Push(SteerToCorner(patrol, start)); // the first command: set off
        state.patrols.push_back(patrol);
    }
    for (std::uint32_t i = 0; i < kCrates; ++i) {
        const ECS::Entity crate = world.Create();
        const double x = (i % 2 == 0) ? kCrateDistanceM : -kCrateDistanceM;
        const double z = (i / 2 == 0) ? kCrateDistanceM : -kCrateDistanceM;
        world.Set(crate, World::WorldTransform{{x, 0.0, z}, World::kNoRotation});
        world.Set(crate, Client::DebugShape{RenderData::ShapeKind::Box, kCrateSize, kCrateOffset,
                                            RenderData::kDebugPhysics});
        world.Set(crate, Client::RenderIdentity{Core::RenderInstanceId(kFirstCrateId + i)});
    }

    // The driver, a 20 Hz producer: reads where each walker is and queues a command when it
    // reaches its corner. It never changes the world itself.
    State *demo = &state;
    ECS::EntityWorld *entities = &world;
    runtime.AddSystem({"engine:demo_driver", Simulation::ScheduleSlot::Hz20Producers,
                       [demo, entities](const Simulation::TickContext &) {
                           for (Patrol &patrol : demo->patrols) {
                               const WorldPosition at =
                                   entities->Get<World::WorldTransform>(patrol.walker).position;
                               if (FlatDistance(at, CornerOf(patrol)) < kArrivedM) {
                                   patrol.corner = (patrol.corner + 1) % kCorners;
                                   demo->steering.Push(SteerToCorner(patrol, at));
                               }
                           }
                       }});
    // The Commands phase: the only place a walker's velocity changes.
    runtime.AddSystem({"engine:demo_steering", Simulation::ScheduleSlot::Hz30Realtime,
                       [demo, entities](const Simulation::TickContext &) {
                           demo->steering.Drain([entities](const Steer &steer) {
                               entities->Get<DemoVelocity>(steer.walker).value = steer.velocity;
                           });
                       },
                       Simulation::RealtimePhase::Commands});
    // The Movement phase: every walker moves by its velocity and faces where it goes.
    const auto moving =
        world.MakeQuery<World::WorldTransform, const DemoVelocity>(kMovingQueryName);
    runtime.AddSystem({"engine:demo_movement", Simulation::ScheduleSlot::Hz30Realtime,
                       [demo, moving](const Simulation::TickContext &context) {
                           moving.Each([&](ECS::Entity, World::WorldTransform &transform,
                                           const DemoVelocity &velocity) {
                               const Vec3 step = velocity.value * context.dt;
                               transform.position = transform.position + step;
                               const float speed = glm::length(velocity.value);
                               if (speed > 0.0f) {
                                   const float yaw =
                                       std::atan2(-velocity.value.x, -velocity.value.z);
                                   transform.rotation =
                                       glm::angleAxis(yaw, kUp); // facing -Z turned to go there
                               }
                               demo->walkedM += static_cast<double>(glm::length(step));
                           });
                       },
                       Simulation::RealtimePhase::Movement});
}

MovingEntitiesLayer::~MovingEntitiesLayer() = default;

void MovingEntitiesLayer::OnUpdate(const Application::FrameTime &time) {
    const double orbit = kTwoPi * Core::ToSeconds(time.sinceStart) / kCameraOrbitPeriodS;
    const WorldPosition eye{kPlaneCenter.x + std::cos(orbit) * kCameraOrbitRadiusM, kCameraHeightM,
                            kPlaneCenter.z + std::sin(orbit) * kCameraOrbitRadiusM};
    m_State->camera.LookAt(eye, kPlaneCenter);
}

void MovingEntitiesLayer::OnDetach() {
    const State &state = *m_State;
    RUNIC_LOG_INFO("RUNIC: moving entities: {} walkers steered by {} commands, {:.1f} m walked",
                   state.patrols.size(), state.steering.Describe().applied, state.walkedM);
}

} // namespace Runic::Programs
