// PHYS-1: the PhysicsWorld on Jolt in double precision, collision layers and shapes, contacts as
// facts.

#include "PhysicsWorld.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <span>
#include <string>
#include <vector>

using namespace Runic;
using namespace Runic::Physics;
using Core::Vec3;
using Core::WorldPosition;

namespace {

constexpr float kTickSeconds = 1.0f / 30.0f; // the realtime domain's step
constexpr int kTwoSeconds = 60;
constexpr float kCrateHalfM = 0.5f;
// A resting body sinks into what holds it by up to Jolt's penetration slop (PhysicsSettings, 2 cm).
constexpr double kPenetrationSlopM = 0.02;
constexpr double kSlopToleranceM = 0.001;

// Thousands of kilometres out, where a 32-bit float's step is about half a metre.
const WorldPosition kFarAway{5'000'000.0, 120.0, -7'000'000.0};

struct Scene {
    BodyHandle ground;
    BodyHandle crate;
};

// A 100 m static slab with its top at origin.y, and a 1 m crate dropped from 3 m above it.
Scene DropACrate(PhysicsWorld &world, WorldPosition origin) {
    const ShapeId slab =
        world.AddShape("engine:test_slab", BoxShape(Vec3(50.0f, 1.0f, 50.0f))).Value();
    const ShapeId crate = world.AddShape("engine:test_crate", BoxShape(Vec3(kCrateHalfM))).Value();
    BodyDesc ground;
    ground.shape = slab;
    ground.motion = MotionType::Static;
    ground.layer = world.Layers().Find(kStaticLayerName);
    ground.pose.position = {origin.x, origin.y - 1.0, origin.z};
    ground.owner = 1;
    BodyDesc falling;
    falling.shape = crate;
    falling.motion = MotionType::Dynamic;
    falling.layer = world.Layers().Find(kMovingLayerName);
    falling.pose.position = {origin.x + 0.25, origin.y + 3.0, origin.z - 0.75};
    falling.owner = 2;
    return {world.CreateBody(ground), world.CreateBody(falling)};
}

// The crate's height above the slab after two seconds, and how much it moved over the last second.
struct Rest {
    double height = 0.0;
    double drift = 0.0;
};

Rest Settle(PhysicsWorld &world, const Scene &scene, WorldPosition origin) {
    double lowest = 1e9;
    double highest = -1e9;
    for (int tick = 0; tick < kTwoSeconds; ++tick) {
        world.Step(kTickSeconds);
        if (tick >= kTwoSeconds / 2) {
            const double y = world.BodyPose(scene.crate).position.y;
            lowest = std::min(lowest, y);
            highest = std::max(highest, y);
        }
    }
    return {world.BodyPose(scene.crate).position.y - origin.y, highest - lowest};
}

int ThreadsOfThisProcess() {
    std::ifstream status("/proc/self/status");
    for (std::string line; std::getline(status, line);) {
        if (line.rfind("Threads:", 0) == 0) {
            return std::stoi(line.substr(line.find(':') + 1));
        }
    }
    return -1;
}

} // namespace

TEST(CollisionLayers, TheEnginesLayersCollideAsDocumented) {
    const CollisionLayers layers = DefaultCollisionLayers();
    ASSERT_EQ(layers.Count(), 4u);
    const CollisionLayer still = layers.Find(kStaticLayerName);
    const CollisionLayer moving = layers.Find(kMovingLayerName);
    const CollisionLayer character = layers.Find(kCharacterLayerName);
    const CollisionLayer sensor = layers.Find(kSensorLayerName);
    EXPECT_EQ(layers.Describe(still).group, BroadPhaseGroup::Static);
    EXPECT_EQ(layers.Describe(character).group, BroadPhaseGroup::Moving);
    EXPECT_FALSE(layers.Collides(still, still));
    EXPECT_TRUE(layers.Collides(still, moving));
    EXPECT_TRUE(layers.Collides(moving, still)); // symmetric
    EXPECT_TRUE(layers.Collides(moving, moving));
    EXPECT_FALSE(layers.Collides(character, character));
    EXPECT_TRUE(layers.Collides(character, sensor));
    EXPECT_FALSE(layers.Collides(sensor, sensor));
    EXPECT_FALSE(layers.Find("engine:test_missing").IsValid());
}

TEST(CollisionLayers, AGameAddsItsOwnLayers) {
    CollisionLayers layers = DefaultCollisionLayers();
    const CollisionLayer ghosts = layers.Add("game:test_ghosts", BroadPhaseGroup::Moving);
    EXPECT_EQ(layers.Count(), 5u);
    EXPECT_FALSE(layers.Collides(ghosts, layers.Find(kStaticLayerName)));
    layers.SetCollides(ghosts, layers.Find(kStaticLayerName), true);
    EXPECT_TRUE(layers.Collides(ghosts, layers.Find(kStaticLayerName)));
    layers.SetCollides(ghosts, layers.Find(kStaticLayerName), false);
    EXPECT_FALSE(layers.Collides(layers.Find(kStaticLayerName), ghosts));
}

TEST(PhysicsShapes, AreRegisteredOnceByNameAndCheckedForSense) {
    PhysicsWorld world;
    const auto box = world.AddShape("engine:test_box", BoxShape(Vec3(1.0f, 2.0f, 3.0f)));
    ASSERT_TRUE(box) << box.GetError().message;
    EXPECT_EQ(world.FindShape("engine:test_box"), box.Value());
    EXPECT_FALSE(world.AddShape("engine:test_box", SphereShape(1.0f))); // taken
    EXPECT_FALSE(world.AddShape("test_box", SphereShape(1.0f)));        // no namespace
    EXPECT_FALSE(world.AddShape("engine:test_flat", BoxShape(Vec3(1.0f, 0.0f, 1.0f))));
    EXPECT_FALSE(world.AddShape("engine:test_dot", SphereShape(0.0f)));
    EXPECT_TRUE(world.AddShape("engine:test_capsule", CapsuleShape(0.5f, 0.4f)));
    EXPECT_FALSE(world.AddShape("engine:test_broken_mesh", MeshShape({Vec3(0.0f)}, {0, 1, 2})));
    const auto mesh = world.AddShape("engine:test_quad",
                                     MeshShape({Vec3(-1.0f, 0.0f, -1.0f), Vec3(1.0f, 0.0f, -1.0f),
                                                Vec3(1.0f, 0.0f, 1.0f), Vec3(-1.0f, 0.0f, 1.0f)},
                                               {0, 2, 1, 0, 3, 2}));
    EXPECT_TRUE(mesh) << mesh.GetError().message;
    EXPECT_EQ(world.Stats().shapes, 3u);
}

TEST(PhysicsWorld, ACrateFallsAndRestsOnTheGround) {
    PhysicsWorld world;
    const Scene scene = DropACrate(world, WorldPosition{});
    EXPECT_TRUE(world.IsActive(scene.crate));
    const Rest rest = Settle(world, scene, WorldPosition{});
    // Its centre half a metre above the slab, less Jolt's slop.
    EXPECT_GE(rest.height, kCrateHalfM - kPenetrationSlopM - kSlopToleranceM);
    EXPECT_LE(rest.height, kCrateHalfM + kSlopToleranceM);
    EXPECT_LT(rest.drift, 1e-4);
    EXPECT_EQ(world.Stats().steps, static_cast<std::uint64_t>(kTwoSeconds));
}

TEST(PhysicsWorld, BodiesCollideWithoutJitterFarFromTheOrigin) {
    // The same drop at the origin and 8,600 km from it rests at the same height and stays still:
    // double precision.
    PhysicsWorld near;
    const Scene nearScene = DropACrate(near, WorldPosition{});
    const Rest atOrigin = Settle(near, nearScene, WorldPosition{});

    PhysicsWorld far;
    const Scene farScene = DropACrate(far, kFarAway);
    const Rest farOut = Settle(far, farScene, kFarAway);

    EXPECT_NEAR(farOut.height, atOrigin.height, 1e-4);
    EXPECT_LT(farOut.drift, 1e-4); // still to a tenth of a millimetre
    const Core::WorldPosition rested = far.BodyPose(farScene.crate).position;
    EXPECT_NEAR(rested.x - kFarAway.x, 0.25, 0.05); // and where it was dropped
    EXPECT_NEAR(rested.z - kFarAway.z, -0.75, 0.05);
}

TEST(PhysicsWorld, CreatesAndRemovesBodiesInBatches) {
    PhysicsWorld world;
    const ShapeId ball = world.AddShape("engine:test_ball", SphereShape(0.5f)).Value();
    std::vector<BodyDesc> descs(1000);
    for (std::size_t i = 0; i < descs.size(); ++i) {
        descs[i].shape = ball;
        descs[i].motion = MotionType::Dynamic;
        descs[i].layer = world.Layers().Find(kMovingLayerName);
        descs[i].pose.position = {static_cast<double>(i) * 2.0, 10.0, 0.0};
    }
    std::vector<BodyHandle> bodies(descs.size());
    world.CreateBodies(descs, bodies);
    EXPECT_EQ(world.Stats().bodies, 1000u);
    EXPECT_TRUE(
        std::all_of(bodies.begin(), bodies.end(), [&](BodyHandle b) { return world.IsBody(b); }));

    world.RemoveBodies(std::span(bodies).first(400));
    EXPECT_EQ(world.Stats().bodies, 600u);
    EXPECT_FALSE(world.IsBody(bodies[0]));
    EXPECT_TRUE(world.IsBody(bodies[400]));
    world.RemoveBodies(std::span(bodies).first(400)); // already gone: ignored
    EXPECT_EQ(world.Stats().bodies, 600u);
}

TEST(PhysicsWorld, AKinematicBodyReachesItsTargetAndPushes) {
    PhysicsWorld world;
    const ShapeId plate =
        world.AddShape("engine:test_plate", BoxShape(Vec3(2.0f, 0.1f, 2.0f))).Value();
    BodyDesc lift;
    lift.shape = plate;
    lift.motion = MotionType::Kinematic;
    lift.layer = world.Layers().Find(kMovingLayerName);
    lift.pose.position = kFarAway;
    const BodyHandle platform = world.CreateBody(lift);
    Pose up;
    up.position = {kFarAway.x, kFarAway.y + 0.5, kFarAway.z};
    world.MoveKinematic(platform, up, kTickSeconds);
    world.Step(kTickSeconds);
    EXPECT_NEAR(world.BodyPose(platform).position.y - kFarAway.y, 0.5, 1e-6);
    EXPECT_NEAR(world.LinearVelocity(platform).y, 0.5f / kTickSeconds, 1e-3f);
}

TEST(PhysicsWorld, ContactsArriveAsFactsWithTheirOwners) {
    PhysicsWorld world;
    const Scene scene = DropACrate(world, kFarAway);
    std::vector<ContactFact> facts;
    for (int tick = 0; tick < kTwoSeconds && facts.empty(); ++tick) {
        world.Step(kTickSeconds);
        world.TakeContacts(facts);
    }
    ASSERT_EQ(facts.size(), 1u); // one fact per pair, however many points touch
    const ContactFact &began = facts[0];
    EXPECT_EQ(began.phase, ContactPhase::Began);
    const bool crateFirst = began.a == scene.crate;
    EXPECT_EQ(crateFirst ? began.ownerA : began.ownerB, 2u);
    EXPECT_EQ(crateFirst ? began.ownerB : began.ownerA, 1u);
    EXPECT_NEAR(began.point.y, kFarAway.y, 0.05); // at the slab's top, far from the origin
    EXPECT_NEAR(std::abs(began.normal.y), 1.0f, 1e-3f);

    // Lifting the crate away ends the contact.
    facts.clear();
    world.SetLinearVelocity(scene.crate, Vec3(0.0f, 20.0f, 0.0f));
    for (int tick = 0; tick < kTwoSeconds && facts.empty(); ++tick) {
        world.Step(kTickSeconds);
        world.TakeContacts(facts);
    }
    ASSERT_EQ(facts.size(), 1u);
    EXPECT_EQ(facts[0].phase, ContactPhase::Ended);
}

TEST(PhysicsWorld, AContactLastsWhileItsBodiesSleep) {
    // Jolt drops the contacts of a body that falls asleep; the crate still rests on the slab, so
    // nothing ended. Waking it and lifting it away ends it; so does removing it while it sleeps.
    PhysicsWorld world;
    const Scene scene = DropACrate(world, kFarAway);
    std::vector<ContactFact> facts;
    constexpr int kUntilAsleep = 3 * kTwoSeconds;
    for (int tick = 0; tick < kUntilAsleep; ++tick) {
        world.Step(kTickSeconds);
        world.TakeContacts(facts);
    }
    ASSERT_FALSE(world.IsActive(scene.crate)); // asleep
    ASSERT_EQ(facts.size(), 1u);
    EXPECT_EQ(facts[0].phase, ContactPhase::Began);

    // A nudge wakes it on the slab: still touching, nothing new.
    facts.clear();
    world.SetLinearVelocity(scene.crate, Vec3(0.1f, 0.0f, 0.0f));
    for (int tick = 0; tick < kTwoSeconds / 2; ++tick) {
        world.Step(kTickSeconds);
        world.TakeContacts(facts);
    }
    EXPECT_TRUE(facts.empty());

    // Asleep again, then removed: the contact ends, with the owners it had.
    for (int tick = 0; tick < kUntilAsleep; ++tick) {
        world.Step(kTickSeconds);
        world.TakeContacts(facts);
    }
    ASSERT_FALSE(world.IsActive(scene.crate));
    EXPECT_TRUE(facts.empty());
    world.RemoveBodies(std::span(&scene.crate, 1));
    world.Step(kTickSeconds);
    world.TakeContacts(facts);
    ASSERT_EQ(facts.size(), 1u);
    EXPECT_EQ(facts[0].phase, ContactPhase::Ended);
    EXPECT_EQ(facts[0].ownerA + facts[0].ownerB, 3u); // the slab's 1 and the crate's 2
}

TEST(PhysicsWorld, JoltCreatesNoThread) {
    const int before = ThreadsOfThisProcess();
    if (before < 0) {
        GTEST_SKIP() << "no /proc/self/status here";
    }
    PhysicsWorld world;
    const Scene scene = DropACrate(world, kFarAway);
    Settle(world, scene, kFarAway);
    EXPECT_EQ(ThreadsOfThisProcess(), before);
}

#if RUNIC_ASSERTS_ENABLED

TEST(PhysicsWorldDeathTest, ABodyNeedsAShapeAndALayerOfTheWorld) {
    PhysicsWorld world;
    BodyDesc nothing;
    EXPECT_DEATH(world.CreateBody(nothing), "names a shape the world does not have");
    nothing.shape = world.AddShape("engine:test_box", BoxShape(Vec3(1.0f))).Value();
    EXPECT_DEATH(world.CreateBody(nothing), "names a collision layer");
}

TEST(CollisionLayersDeathTest, ALayerNameIsUniqueAndHasANamespace) {
    CollisionLayers layers;
    layers.Add("engine:test_a", BroadPhaseGroup::Moving);
    EXPECT_DEATH(layers.Add("engine:test_a", BroadPhaseGroup::Static), "share a name");
    EXPECT_DEATH(layers.Add("plain", BroadPhaseGroup::Static), "namespace:name");
}

#endif // RUNIC_ASSERTS_ENABLED
