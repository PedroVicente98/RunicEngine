// SIM-8: the physics bridge. Entities hold transforms and opaque Physics handles; bodies follow
// their entities at the barriers; a sync on each side of the step; contacts arrive as facts.

#include "PhysicsBridge.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

#include "EntityWorld.hpp"
#include "EventBuffer.hpp"
#include "SimulationRuntime.hpp"
#include "WorldComponents.hpp"

using namespace Runic;
using namespace Runic::Simulation;
using Core::Vec3;
using Core::WorldPosition;

namespace {

// Thousands of kilometres out, where a 32-bit float's step is about half a metre.
const WorldPosition kFarAway{5'000'000.0, 120.0, -7'000'000.0};
constexpr float kWalkMps = 2.0f;
constexpr int kTwoSeconds = 60; // realtime ticks

// Runs master ticks until the realtime slot has run once more (tick 0 and every even tick).
void RunRealtimeTick(SimulationRuntime &runtime) {
    const std::uint64_t before = runtime.TicksRun(TickDomain::Hz30);
    while (runtime.TicksRun(TickDomain::Hz30) == before) {
        runtime.RunMasterTick();
    }
}

void AddRealtime(SimulationRuntime &runtime, RealtimePhase phase, SystemFunction run) {
    runtime.AddSystem({"engine:test_system", ScheduleSlot::Hz30Realtime, std::move(run), phase});
}

WorldPosition At(WorldPosition origin, double x, double y, double z) {
    return {origin.x + x, origin.y + y, origin.z + z};
}

ECS::Entity Place(SimulationRuntime &runtime, WorldPosition position) {
    ECS::EntityWorld &entities = runtime.Entities();
    const ECS::Entity entity = entities.Create();
    entities.Set(entity, World::WorldTransform{position});
    return entity;
}

// A 100 m static floor whose top is at the origin's height, as an entity.
ECS::Entity Floor(SimulationRuntime &runtime, PhysicsBridge &bridge, WorldPosition origin) {
    Physics::BodyDesc desc;
    desc.shape = bridge.World()
                     .AddShape("engine:test_floor", Physics::BoxShape(Vec3(50.0f, 1.0f, 50.0f)))
                     .Value();
    desc.layer = bridge.World().Layers().Find(Physics::kStaticLayerName);
    const ECS::Entity floor = Place(runtime, At(origin, 0.0, -1.0, 0.0));
    bridge.AddBody(floor, desc);
    return floor;
}

// A 1 m dynamic crate dropped from 3 m above the origin.
ECS::Entity Crate(SimulationRuntime &runtime, PhysicsBridge &bridge, WorldPosition origin) {
    Physics::BodyDesc desc;
    desc.shape =
        bridge.World().AddShape("engine:test_crate", Physics::BoxShape(Vec3(0.5f))).Value();
    desc.motion = Physics::MotionType::Dynamic;
    desc.layer = bridge.World().Layers().Find(Physics::kMovingLayerName);
    const ECS::Entity crate = Place(runtime, At(origin, 0.0, 3.0, 0.0));
    bridge.AddBody(crate, desc);
    return crate;
}

ECS::Entity Walker(SimulationRuntime &runtime, PhysicsBridge &bridge, WorldPosition feet) {
    Physics::CharacterDesc desc;
    desc.layer = bridge.World().Layers().Find(Physics::kCharacterLayerName);
    const ECS::Entity walker = Place(runtime, feet);
    bridge.AddCharacter(walker, desc);
    return walker;
}

} // namespace

TEST(PhysicsBridge, TheCapsulesTransformMatchesItsCharacterAfterEveryStep) {
    SimulationRuntime runtime;
    PhysicsBridge &bridge = runtime.EnablePhysics();
    EXPECT_EQ(runtime.FindPhysicsBridge(), &bridge);
    Floor(runtime, bridge, kFarAway);
    const ECS::Entity walker = Walker(runtime, bridge, kFarAway);
    ECS::EntityWorld &entities = runtime.Entities();

    // Gameplay moves the character in the Movement phase; a check after the write-back compares.
    AddRealtime(runtime, RealtimePhase::Movement, [&](const TickContext &) {
        if (PhysicsCharacter *character = entities.TryGet<PhysicsCharacter>(walker)) {
            character->move.velocity = Vec3(kWalkMps, 0.0f, 0.0f);
        }
    });
    int checked = 0;
    AddRealtime(runtime, RealtimePhase::Queries, [&](const TickContext &) {
        const PhysicsCharacter &character = entities.Get<PhysicsCharacter>(walker);
        const WorldPosition feet = bridge.World().Character(character.handle).pose.position;
        const WorldPosition transform = entities.Get<World::WorldTransform>(walker).position;
        EXPECT_EQ(transform.x, feet.x); // exactly, every step
        EXPECT_EQ(transform.y, feet.y);
        EXPECT_EQ(transform.z, feet.z);
        ++checked;
    });
    for (int tick = 0; tick < kTwoSeconds; ++tick) {
        RunRealtimeTick(runtime);
    }
    EXPECT_EQ(checked, kTwoSeconds);
    // It walked about 4 m in two seconds (less the first tick, before it had its character).
    const WorldPosition end = entities.Get<World::WorldTransform>(walker).position;
    EXPECT_NEAR(end.x - kFarAway.x, kWalkMps * (kTwoSeconds - 1) / 30.0, 0.03);
    EXPECT_NEAR(end.y - kFarAway.y, 0.0, 0.03);
    // The move lasts one step: the bridge cleared it.
    EXPECT_EQ(entities.Get<PhysicsCharacter>(walker).move.velocity.x, 0.0f);
}

TEST(PhysicsBridge, BodiesArriveInThePhysicsStructurePhase) {
    SimulationRuntime runtime;
    PhysicsBridge &bridge = runtime.EnablePhysics();
    const ECS::Entity floor = Floor(runtime, bridge, WorldPosition{});
    ECS::EntityWorld &entities = runtime.Entities();
    EXPECT_EQ(bridge.Stats().pending, 1u);

    std::vector<bool> seenIn; // Movement, then PhysicsStep, of the first tick
    AddRealtime(runtime, RealtimePhase::Movement,
                [&](const TickContext &) { seenIn.push_back(entities.Has<PhysicsBody>(floor)); });
    AddRealtime(runtime, RealtimePhase::PhysicsStep,
                [&](const TickContext &) { seenIn.push_back(entities.Has<PhysicsBody>(floor)); });
    RunRealtimeTick(runtime);

    EXPECT_EQ(seenIn, (std::vector<bool>{false, true}));
    const PhysicsBody &body = entities.Get<PhysicsBody>(floor);
    EXPECT_TRUE(bridge.World().IsBody(body.handle));
    EXPECT_EQ(body.motion, Physics::MotionType::Static);
    // At its entity's transform, owned by it.
    EXPECT_EQ(bridge.World().BodyPose(body.handle).position.y, -1.0);
    EXPECT_EQ(bridge.Stats().bodies, 1u);
    EXPECT_EQ(bridge.Stats().pending, 0u);
}

TEST(PhysicsBridge, DestroyingAnEntityRemovesItsBodyAtTheNextBarrier) {
    SimulationRuntime runtime;
    PhysicsBridge &bridge = runtime.EnablePhysics();
    Floor(runtime, bridge, WorldPosition{});
    const ECS::Entity crate = Crate(runtime, bridge, WorldPosition{});
    const ECS::Entity walker = Walker(runtime, bridge, At(WorldPosition{}, 3.0, 0.0, 0.0));
    ECS::EntityWorld &entities = runtime.Entities();
    RunRealtimeTick(runtime);
    const Physics::BodyHandle body = entities.Get<PhysicsBody>(crate).handle;
    const Physics::CharacterHandle character = entities.Get<PhysicsCharacter>(walker).handle;

    // Destroyed in the Queries phase: the destruction applies at its barrier, and so does the
    // body's removal, so the Commit phase already sees neither.
    bool destroy = true;
    std::vector<bool> bodyInCommit;
    AddRealtime(runtime, RealtimePhase::Queries, [&](const TickContext &) {
        if (destroy) {
            entities.Destroy(crate);
            destroy = false;
        }
    });
    AddRealtime(runtime, RealtimePhase::Commit,
                [&](const TickContext &) { bodyInCommit.push_back(bridge.World().IsBody(body)); });
    RunRealtimeTick(runtime);
    EXPECT_EQ(bodyInCommit, (std::vector<bool>{false}));
    EXPECT_EQ(bridge.Stats().bodies, 1u); // the floor
    EXPECT_EQ(bridge.Stats().followedDestroyed, 1u);

    // Destroyed between steps: gone at the barrier that opens the next step, before any phase.
    entities.Destroy(walker);
    std::vector<bool> characterInCommands;
    AddRealtime(runtime, RealtimePhase::Commands, [&](const TickContext &) {
        characterInCommands.push_back(bridge.World().IsCharacter(character));
    });
    RunRealtimeTick(runtime);
    EXPECT_EQ(characterInCommands, (std::vector<bool>{false}));
    EXPECT_EQ(bridge.Stats().characters, 0u);
    EXPECT_EQ(bridge.Stats().followedDestroyed, 2u);
}

TEST(PhysicsBridge, KinematicTargetsGoInAndDynamicPosesComeOut) {
    SimulationRuntime runtime;
    PhysicsBridge &bridge = runtime.EnablePhysics();
    Floor(runtime, bridge, kFarAway);
    const ECS::Entity crate = Crate(runtime, bridge, kFarAway);
    Physics::BodyDesc door;
    door.shape = bridge.World().AddShape("engine:test_door", Physics::BoxShape(Vec3(0.5f))).Value();
    door.motion = Physics::MotionType::Kinematic;
    door.layer = bridge.World().Layers().Find(Physics::kMovingLayerName);
    const ECS::Entity mover = Place(runtime, At(kFarAway, 10.0, 2.0, 0.0));
    bridge.AddBody(mover, door);
    ECS::EntityWorld &entities = runtime.Entities();

    // Gameplay slides the kinematic entity 1 m/s along +Z in the Movement phase.
    AddRealtime(runtime, RealtimePhase::Movement, [&](const TickContext &context) {
        entities.Get<World::WorldTransform>(mover).position.z += context.dt;
    });
    AddRealtime(runtime, RealtimePhase::Queries, [&](const TickContext &) {
        const Physics::Pose crateBody =
            bridge.World().BodyPose(entities.Get<PhysicsBody>(crate).handle);
        const World::WorldTransform &crateTransform = entities.Get<World::WorldTransform>(crate);
        EXPECT_EQ(crateTransform.position.y, crateBody.position.y); // written back exactly
        EXPECT_EQ(crateTransform.rotation, crateBody.rotation);
        const Physics::Pose doorBody =
            bridge.World().BodyPose(entities.Get<PhysicsBody>(mover).handle);
        EXPECT_NEAR(doorBody.position.z, entities.Get<World::WorldTransform>(mover).position.z,
                    1e-6); // reached its target
    });
    for (int tick = 0; tick < kTwoSeconds; ++tick) {
        RunRealtimeTick(runtime);
    }
    // The crate fell and rests on the floor; the door went where its entity went.
    EXPECT_NEAR(entities.Get<World::WorldTransform>(crate).position.y - kFarAway.y, 0.5, 0.025);
    EXPECT_NEAR(entities.Get<World::WorldTransform>(mover).position.z - kFarAway.z, 2.0, 1e-3);
}

TEST(PhysicsBridge, AContactReachesTheNextPhaseAsAFact) {
    SimulationRuntime runtime;
    PhysicsBridge &bridge = runtime.EnablePhysics();
    const ECS::Entity floor = Floor(runtime, bridge, WorldPosition{});
    const ECS::Entity crate = Crate(runtime, bridge, WorldPosition{});
    Events::EventReader<PhysicsContact> inPostPhysics = bridge.Contacts().MakeReader();
    Events::EventReader<PhysicsContact> inQueries = bridge.Contacts().MakeReader();

    std::vector<std::uint64_t> landedOn; // the realtime ticks a contact was read in Queries
    bool readInPostPhysics = false;
    AddRealtime(runtime, RealtimePhase::PostPhysics, [&](const TickContext &) {
        // After the bridge's write-back in the same phase: its facts are not readable yet.
        readInPostPhysics = readInPostPhysics || !inPostPhysics.Take().empty();
    });
    AddRealtime(runtime, RealtimePhase::Queries, [&](const TickContext &context) {
        for (const Events::Event<PhysicsContact> &event : inQueries.Take()) {
            EXPECT_EQ(event.fact.phase, Physics::ContactPhase::Began);
            const bool pair = (event.fact.a == floor && event.fact.b == crate) ||
                              (event.fact.a == crate && event.fact.b == floor);
            EXPECT_TRUE(pair);
            landedOn.push_back(context.domainTick);
        }
    });
    for (int tick = 0; tick < kTwoSeconds; ++tick) {
        RunRealtimeTick(runtime);
        (void)inPostPhysics.Take(); // catch up between steps: what it missed in its phase
    }
    ASSERT_EQ(landedOn.size(), 1u); // one fact for the pair, read once, in the next phase
    EXPECT_GT(landedOn.front(), 0u);
    EXPECT_FALSE(readInPostPhysics);
}

TEST(PhysicsBridge, RemoveRequestsTakeBodiesAndDropWaitingRequests) {
    SimulationRuntime runtime;
    PhysicsBridge &bridge = runtime.EnablePhysics();
    const ECS::Entity floor = Floor(runtime, bridge, WorldPosition{});
    ECS::EntityWorld &entities = runtime.Entities();
    RunRealtimeTick(runtime);
    const Physics::BodyHandle body = entities.Get<PhysicsBody>(floor).handle;

    bridge.Remove(floor);
    RunRealtimeTick(runtime);
    EXPECT_FALSE(bridge.World().IsBody(body));
    EXPECT_FALSE(entities.Has<PhysicsBody>(floor));
    EXPECT_TRUE(entities.IsAlive(floor)); // only its body went
    EXPECT_EQ(bridge.Stats().bodies, 0u);

    // Asked for and taken back before the PhysicsStructure phase: nothing is made.
    const ECS::Entity walker = Walker(runtime, bridge, At(WorldPosition{}, 3.0, 0.0, 0.0));
    bridge.Remove(walker);
    EXPECT_EQ(bridge.Stats().pending, 0u);
    RunRealtimeTick(runtime);
    EXPECT_FALSE(entities.Has<PhysicsCharacter>(walker));
    EXPECT_EQ(bridge.World().Stats().characters, 0u);
}

TEST(PhysicsBridge, AnEntityGetsATransformFromItsDescriptionWhenItHasNone) {
    SimulationRuntime runtime;
    PhysicsBridge &bridge = runtime.EnablePhysics();
    Physics::BodyDesc desc;
    desc.shape =
        bridge.World().AddShape("engine:test_post", Physics::BoxShape(Vec3(0.25f))).Value();
    desc.layer = bridge.World().Layers().Find(Physics::kStaticLayerName);
    desc.pose.position = {4.0, 5.0, 6.0};
    ECS::EntityWorld &entities = runtime.Entities();
    const ECS::Entity post = entities.Create();
    bridge.AddBody(post, desc);
    RunRealtimeTick(runtime);
    EXPECT_EQ(entities.Get<World::WorldTransform>(post).position.y, 5.0);
    EXPECT_EQ(bridge.World().BodyPose(entities.Get<PhysicsBody>(post).handle).position.z, 6.0);
}

#if RUNIC_ASSERTS_ENABLED

TEST(PhysicsBridgeDeathTest, IsEnabledOnceAndGivesAnEntityOneBody) {
    SimulationRuntime runtime;
    PhysicsBridge &bridge = runtime.EnablePhysics();
    EXPECT_DEATH(runtime.EnablePhysics(), "physics is already enabled");
    const ECS::Entity floor = Floor(runtime, bridge, WorldPosition{});
    EXPECT_DEATH(bridge.AddBody(floor, Physics::BodyDesc{}), "already has a body or a character");
    EXPECT_DEATH(bridge.AddCharacter(floor, Physics::CharacterDesc{}),
                 "already has a body or a character");
}

#endif // RUNIC_ASSERTS_ENABLED
