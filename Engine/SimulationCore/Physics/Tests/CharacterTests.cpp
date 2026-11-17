// PHYS-2: characters (Jolt's CharacterVirtual) walk on walkable ground, climb steps, stop at walls
// and slide along them, fall, jump, ride what they stand on, and do the same far from the origin.

#include "PhysicsWorld.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <string_view>
#include <vector>

using namespace Runic;
using namespace Runic::Physics;
using Core::Quat;
using Core::Vec3;
using Core::WorldPosition;

namespace {

constexpr float kTickSeconds = 1.0f / 30.0f; // the realtime domain's step
constexpr int kOneSecond = 30;
constexpr float kWalkMps = 2.0f;
// A character keeps Jolt's padding (2 cm) between itself and what it touches.
constexpr double kPaddingM = 0.02;
constexpr double kNearM = 0.03; // "about there": padding and Jolt's slop
const WorldPosition kFarAway{5'000'000.0, 120.0, -7'000'000.0};

WorldPosition At(WorldPosition origin, double x, double y, double z) {
    return {origin.x + x, origin.y + y, origin.z + z};
}

Quat AboutZ(float degrees) {
    const float half = degrees * std::numbers::pi_v<float> / 360.0f;
    return {std::cos(half), 0.0f, 0.0f, std::sin(half)};
}

// A static box of half extents `half` whose top is at height `top`, starting at `fromX` along +X.
void Block(PhysicsWorld &world, WorldPosition origin, const char *name, Vec3 half, double fromX,
           double top) {
    const ShapeId shape = world.AddShape(name, BoxShape(half)).Value();
    BodyDesc desc;
    desc.shape = shape;
    desc.layer = world.Layers().Find(kStaticLayerName);
    desc.pose.position = At(origin, fromX + half.x, top - half.y, 0.0);
    world.CreateBody(desc);
}

// A 100 m floor with its top at the origin's height.
void Floor(PhysicsWorld &world, WorldPosition origin) {
    Block(world, origin, "engine:test_floor", Vec3(50.0f, 1.0f, 50.0f), -50.0, 0.0);
}

// A ramp rising along +X from (2, 0) at `degrees`.
void Ramp(PhysicsWorld &world, WorldPosition origin, float degrees) {
    const Vec3 half(10.0f, 0.5f, 5.0f);
    const double angle = degrees * std::numbers::pi / 180.0;
    const ShapeId shape = world.AddShape("engine:test_ramp", BoxShape(half)).Value();
    BodyDesc desc;
    desc.shape = shape;
    desc.layer = world.Layers().Find(kStaticLayerName);
    // The top face's low edge sits at x = 2, y = 0.
    desc.pose.position = At(origin, 2.0 + half.x * std::cos(angle) + half.y * std::sin(angle),
                            half.x * std::sin(angle) - half.y * std::cos(angle), 0.0);
    desc.pose.rotation = AboutZ(degrees);
    world.CreateBody(desc);
}

CharacterHandle Walker(PhysicsWorld &world, WorldPosition feet,
                       std::string_view layer = kCharacterLayerName) {
    CharacterDesc desc;
    desc.layer = world.Layers().Find(layer);
    desc.pose.position = feet;
    desc.owner = 7;
    return world.CreateCharacter(desc).Value();
}

// Moves the character at `velocity` for `ticks` steps; returns where it ends, relative to origin.
Vec3 Walk(PhysicsWorld &world, CharacterHandle character, Vec3 velocity, int ticks,
          WorldPosition origin) {
    for (int tick = 0; tick < ticks; ++tick) {
        world.MoveCharacter(character, {velocity});
        world.Step(kTickSeconds);
    }
    const WorldPosition p = world.Character(character).pose.position;
    return Vec3(static_cast<float>(p.x - origin.x), static_cast<float>(p.y - origin.y),
                static_cast<float>(p.z - origin.z));
}

} // namespace

TEST(Characters, WalkOnFlatGround) {
    PhysicsWorld world;
    Floor(world, WorldPosition{});
    const CharacterHandle walker = Walker(world, WorldPosition{});
    EXPECT_TRUE(world.IsCharacter(walker));
    EXPECT_EQ(world.Stats().characters, 1u);

    const Vec3 end = Walk(world, walker, Vec3(kWalkMps, 0.0f, 0.0f), kOneSecond, WorldPosition{});
    EXPECT_NEAR(end.x, kWalkMps, kNearM); // two metres in a second
    EXPECT_NEAR(end.y, 0.0, kNearM);
    EXPECT_NEAR(end.z, 0.0, 1e-4);
    const CharacterState state = world.Character(walker);
    EXPECT_EQ(state.ground, GroundState::OnGround);
    EXPECT_NEAR(state.groundNormal.y, 1.0f, 1e-3f);
    EXPECT_TRUE(world.IsBody(state.groundBody));
    EXPECT_NEAR(state.velocity.x, kWalkMps, kNearM);

    // Nobody moves it: it stands where it stopped.
    for (int tick = 0; tick < kOneSecond; ++tick) {
        world.Step(kTickSeconds);
    }
    EXPECT_NEAR(world.Character(walker).pose.position.x, end.x, 1e-3);
}

TEST(Characters, ClimbAStepLowerThanTheirStepHeight) {
    PhysicsWorld world;
    Floor(world, WorldPosition{});
    Block(world, WorldPosition{}, "engine:test_step", Vec3(10.0f, 1.0f, 10.0f), 2.0, 0.3);
    const CharacterHandle walker = Walker(world, WorldPosition{});

    const Vec3 end =
        Walk(world, walker, Vec3(kWalkMps, 0.0f, 0.0f), 2 * kOneSecond, WorldPosition{});
    EXPECT_NEAR(end.y, 0.3, kNearM); // on top of the step
    EXPECT_GT(end.x, 3.5);           // and walking on, barely slowed
    EXPECT_EQ(world.Character(walker).ground, GroundState::OnGround);
}

TEST(Characters, StopAtAStepHigherThanTheirStepHeight) {
    PhysicsWorld world;
    Floor(world, WorldPosition{});
    Block(world, WorldPosition{}, "engine:test_ledge", Vec3(10.0f, 1.0f, 10.0f), 2.0, 0.6);
    const CharacterHandle walker = Walker(world, WorldPosition{});

    const Vec3 end =
        Walk(world, walker, Vec3(kWalkMps, 0.0f, 0.0f), 2 * kOneSecond, WorldPosition{});
    EXPECT_NEAR(end.y, 0.0, kNearM);
    EXPECT_NEAR(end.x, 2.0 - kDefaultCharacterRadiusM - kPaddingM, kNearM); // against it
}

TEST(Characters, StopAtWallsAndSlideAlongThem) {
    PhysicsWorld world;
    Floor(world, WorldPosition{});
    Block(world, WorldPosition{}, "engine:test_wall", Vec3(0.5f, 2.0f, 20.0f), 2.0, 4.0);
    const CharacterHandle walker = Walker(world, WorldPosition{});

    const Vec3 stopped =
        Walk(world, walker, Vec3(kWalkMps, 0.0f, 0.0f), 2 * kOneSecond, WorldPosition{});
    const double atTheWall = 2.0 - kDefaultCharacterRadiusM - kPaddingM;
    EXPECT_NEAR(stopped.x, atTheWall, kNearM);
    EXPECT_NEAR(stopped.z, 0.0, 1e-3);

    // Walking into it at 45 degrees keeps the part of the velocity along it.
    const Vec3 slid =
        Walk(world, walker, Vec3(kWalkMps, 0.0f, kWalkMps), kOneSecond, WorldPosition{});
    EXPECT_NEAR(slid.x, atTheWall, kNearM);
    EXPECT_NEAR(slid.z, kWalkMps, 2.0 * kNearM);
}

TEST(Characters, WalkUpWalkableSlopesButNotSteepOnes) {
    {
        PhysicsWorld world;
        Floor(world, WorldPosition{});
        Ramp(world, WorldPosition{}, 20.0f);
        const CharacterHandle walker = Walker(world, WorldPosition{});
        const Vec3 end =
            Walk(world, walker, Vec3(kWalkMps, 0.0f, 0.0f), 3 * kOneSecond, WorldPosition{});
        EXPECT_GT(end.y, 1.0); // well up the ramp
        EXPECT_EQ(world.Character(walker).ground, GroundState::OnGround);
    }
    {
        PhysicsWorld world;
        Floor(world, WorldPosition{});
        Ramp(world, WorldPosition{}, 65.0f); // steeper than the default 50 degrees
        const CharacterHandle walker = Walker(world, WorldPosition{});
        const Vec3 end =
            Walk(world, walker, Vec3(kWalkMps, 0.0f, 0.0f), 3 * kOneSecond, WorldPosition{});
        EXPECT_LT(end.y, kDefaultStepHeightM); // it gets no higher than a step
        EXPECT_LT(end.x, 2.0);
    }
}

TEST(Characters, FallOffEdgesAndLandBelow) {
    PhysicsWorld world;
    // A 4 m platform at the origin's height, over a floor 3 m below.
    Block(world, WorldPosition{}, "engine:test_platform", Vec3(2.0f, 0.5f, 2.0f), -2.0, 0.0);
    Block(world, WorldPosition{}, "engine:test_low_floor", Vec3(50.0f, 1.0f, 50.0f), -50.0, -3.0);
    const CharacterHandle walker = Walker(world, WorldPosition{});

    bool flew = false;
    for (int tick = 0; tick < 3 * kOneSecond; ++tick) {
        world.MoveCharacter(walker, {Vec3(kWalkMps, 0.0f, 0.0f)});
        world.Step(kTickSeconds);
        flew = flew || world.Character(walker).ground == GroundState::InAir;
    }
    EXPECT_TRUE(flew);
    const CharacterState landed = world.Character(walker);
    EXPECT_EQ(landed.ground, GroundState::OnGround);
    EXPECT_NEAR(landed.pose.position.y, -3.0, kNearM);
    EXPECT_GT(landed.pose.position.x, 2.0); // off the edge, not stuck to it
}

TEST(Characters, JumpOnlyFromTheGround) {
    PhysicsWorld world;
    Floor(world, WorldPosition{});
    const CharacterHandle jumper = Walker(world, WorldPosition{});
    world.Step(kTickSeconds); // it finds the ground
    ASSERT_EQ(world.Character(jumper).ground, GroundState::OnGround);

    constexpr float kJumpMps = 5.0f;
    const double expectedApex = kJumpMps * kJumpMps / (2.0 * kStandardGravityMps2); // 1.27 m
    double apex = 0.0;
    bool jumpedAgain = false;
    for (int tick = 0; tick < 2 * kOneSecond; ++tick) {
        // It keeps asking to jump: only the first ask, on the ground, counts until it lands.
        world.MoveCharacter(jumper, {Vec3(0.0f), kJumpMps});
        world.Step(kTickSeconds);
        const double y = world.Character(jumper).pose.position.y;
        if (apex > 0.5 && y < 0.05) {
            jumpedAgain = true; // it landed and may jump again
            break;
        }
        apex = std::max(apex, y);
    }
    EXPECT_NEAR(apex, expectedApex, 0.15); // less a little: gravity acts from the first step
    EXPECT_TRUE(jumpedAgain);
}

TEST(Characters, RideWhatTheyStandOn) {
    // A kinematic platform moves along +X at 1 m/s; the character on it is never moved.
    PhysicsWorld world;
    const ShapeId deck =
        world.AddShape("engine:test_deck", BoxShape(Vec3(3.0f, 0.25f, 3.0f))).Value();
    BodyDesc desc;
    desc.shape = deck;
    desc.motion = MotionType::Kinematic;
    desc.layer = world.Layers().Find(kMovingLayerName);
    desc.pose.position = {0.0, -0.25, 0.0};
    const BodyHandle platform = world.CreateBody(desc);
    const CharacterHandle rider = Walker(world, WorldPosition{});
    world.Step(kTickSeconds); // it finds the deck before the deck moves

    constexpr double kPlatformMps = 1.0;
    for (int tick = 1; tick <= 2 * kOneSecond; ++tick) {
        Pose target = desc.pose;
        target.position.x = kPlatformMps * tick * kTickSeconds;
        world.MoveKinematic(platform, target, kTickSeconds);
        world.Step(kTickSeconds);
    }
    const CharacterState state = world.Character(rider);
    EXPECT_NEAR(world.BodyPose(platform).position.x, 2.0, 1e-3);
    EXPECT_NEAR(state.pose.position.x, 2.0, kNearM); // carried along with it
    EXPECT_NEAR(state.pose.position.y, 0.0, kNearM);
    EXPECT_EQ(state.ground, GroundState::OnGround);
    EXPECT_EQ(state.groundBody, platform);
    EXPECT_NEAR(state.groundVelocity.x, kPlatformMps, 1e-3);
}

TEST(Characters, MoveTheSameFarFromTheOrigin) {
    // The same walk over a step, into a wall and along it, at the origin and 8,600 km from it.
    auto walkTheCourse = [](WorldPosition origin) {
        PhysicsWorld world;
        Floor(world, origin);
        Block(world, origin, "engine:test_step", Vec3(3.0f, 1.0f, 10.0f), 2.0, 0.3);
        Block(world, origin, "engine:test_wall", Vec3(0.5f, 2.0f, 20.0f), 6.0, 4.0);
        const CharacterHandle walker = Walker(world, At(origin, 0.0, 0.0, 0.0));
        std::vector<Vec3> path;
        for (int tick = 0; tick < 4 * kOneSecond; ++tick) {
            path.push_back(Walk(world, walker, Vec3(kWalkMps, 0.0f, 1.0f), 1, origin));
        }
        return path;
    };
    const std::vector<Vec3> near = walkTheCourse(WorldPosition{});
    const std::vector<Vec3> far = walkTheCourse(kFarAway);
    ASSERT_EQ(near.size(), far.size());
    float worst = 0.0f;
    for (std::size_t i = 0; i < near.size(); ++i) {
        worst = std::max({worst, std::abs(near[i].x - far[i].x), std::abs(near[i].y - far[i].y),
                          std::abs(near[i].z - far[i].z)});
    }
    EXPECT_LT(worst, 1e-3f);                 // within a millimetre at every step
    EXPECT_NEAR(near.back().y, 0.3, kNearM); // it did climb the step
    EXPECT_NEAR(near.back().x, 6.0 - kDefaultCharacterRadiusM - kPaddingM, kNearM); // and stop
}

TEST(Characters, MeetEachOtherOnlyWhenTheirLayersCollide) {
    // On the Engine's character layer, characters pass through each other.
    {
        PhysicsWorld world;
        Floor(world, WorldPosition{});
        const CharacterHandle left = Walker(world, At(WorldPosition{}, -1.0, 0.0, 0.0));
        const CharacterHandle right = Walker(world, At(WorldPosition{}, 1.0, 0.0, 0.0));
        for (int tick = 0; tick < 2 * kOneSecond; ++tick) {
            world.MoveCharacter(left, {Vec3(1.0f, 0.0f, 0.0f)});
            world.MoveCharacter(right, {Vec3(-1.0f, 0.0f, 0.0f)});
            world.Step(kTickSeconds);
        }
        EXPECT_NEAR(world.Character(left).pose.position.x, 1.0, kNearM);
        EXPECT_NEAR(world.Character(right).pose.position.x, -1.0, kNearM);
    }
    // On a game's layer that collides with itself, they stop against each other.
    {
        PhysicsWorldSpec spec;
        const CollisionLayer solid =
            spec.layers.Add("engine:test_solid_character", BroadPhaseGroup::Moving);
        spec.layers.SetCollides(solid, solid, true);
        spec.layers.SetCollides(solid, spec.layers.Find(kStaticLayerName), true);
        PhysicsWorld world(spec);
        Floor(world, WorldPosition{});
        const CharacterHandle left =
            Walker(world, At(WorldPosition{}, -1.0, 0.0, 0.0), "engine:test_solid_character");
        const CharacterHandle right =
            Walker(world, At(WorldPosition{}, 1.0, 0.0, 0.0), "engine:test_solid_character");
        for (int tick = 0; tick < 2 * kOneSecond; ++tick) {
            world.MoveCharacter(left, {Vec3(1.0f, 0.0f, 0.0f)});
            world.MoveCharacter(right, {Vec3(-1.0f, 0.0f, 0.0f)});
            world.Step(kTickSeconds);
        }
        const double gap =
            world.Character(right).pose.position.x - world.Character(left).pose.position.x;
        // Shoulder to shoulder: two radii, both paddings and at most a step of their walk apart.
        EXPECT_GT(gap, 2.0 * kDefaultCharacterRadiusM);
        EXPECT_LT(gap, 2.0 * (kDefaultCharacterRadiusM + kPaddingM) + kTickSeconds * 2.0);
    }
}

TEST(Characters, FindEachOtherAcrossTheGridAndAfterTeleports) {
    // Characters are found near each other through a grid of 4 m columns: a walker crossing
    // several of them, and a stander put elsewhere, still meet exactly where they are.
    PhysicsWorldSpec spec;
    const CollisionLayer solid =
        spec.layers.Add("engine:test_solid_character", BroadPhaseGroup::Moving);
    spec.layers.SetCollides(solid, solid, true);
    spec.layers.SetCollides(solid, spec.layers.Find(kStaticLayerName), true);
    PhysicsWorld world(spec);
    Floor(world, WorldPosition{});
    const CharacterHandle walker =
        Walker(world, At(WorldPosition{}, -10.0, 0.0, 0.0), "engine:test_solid_character");
    const CharacterHandle stander =
        Walker(world, At(WorldPosition{}, 1.0, 0.0, 0.0), "engine:test_solid_character");
    const double shoulders = 2.0 * (kDefaultCharacterRadiusM + kPaddingM);

    Vec3 end = Walk(world, walker, Vec3(kWalkMps, 0.0f, 0.0f), 7 * kOneSecond, WorldPosition{});
    EXPECT_NEAR(end.x, 1.0 - shoulders, kNearM); // stopped against it, three columns later
    EXPECT_NEAR(world.Character(stander).pose.position.x, 1.0, kNearM);

    Pose away;
    away.position = {20.0, 0.0, 0.0};
    world.SetCharacterPose(stander, away);
    end = Walk(world, walker, Vec3(kWalkMps, 0.0f, 0.0f), kOneSecond, WorldPosition{});
    EXPECT_NEAR(end.x, 1.0 - shoulders + kWalkMps, kNearM); // nothing in the way now

    Pose ahead;
    ahead.position = {end.x + 1.0, 0.0, 0.0};
    world.SetCharacterPose(stander, ahead);
    const Vec3 blocked =
        Walk(world, walker, Vec3(kWalkMps, 0.0f, 0.0f), kOneSecond, WorldPosition{});
    EXPECT_NEAR(blocked.x, ahead.position.x - shoulders, kNearM);
}

TEST(Characters, AreMadeAndRemovedByHandle) {
    PhysicsWorld world;
    Floor(world, WorldPosition{});
    const CharacterHandle first = Walker(world, WorldPosition{});
    const CharacterHandle second = Walker(world, At(WorldPosition{}, 3.0, 0.0, 0.0));
    EXPECT_NE(first, second);
    EXPECT_EQ(world.Character(second).pose.position.x, 3.0);

    world.RemoveCharacter(first);
    EXPECT_FALSE(world.IsCharacter(first));
    EXPECT_TRUE(world.IsCharacter(second));
    EXPECT_EQ(world.Stats().characters, 1u);
    world.RemoveCharacter(first);             // already gone: ignored
    world.RemoveCharacter(CharacterHandle()); // names nothing: ignored

    // A new character takes the free place under a new handle: the old one stays dead.
    const CharacterHandle third = Walker(world, WorldPosition{});
    EXPECT_NE(third, first);
    EXPECT_FALSE(world.IsCharacter(first));
    EXPECT_TRUE(world.IsCharacter(third));
    world.Step(kTickSeconds);

    // A teleport puts it there at once.
    Pose there;
    there.position = {10.0, 0.0, -4.0};
    world.SetCharacterPose(third, there);
    EXPECT_EQ(world.Character(third).pose.position.z, -4.0);
}

TEST(Characters, RefuseDegenerateDescriptions) {
    PhysicsWorld world;
    CharacterDesc desc;
    desc.layer = world.Layers().Find(kCharacterLayerName);
    EXPECT_TRUE(world.CreateCharacter(desc).HasValue());

    CharacterDesc flat = desc;
    flat.height = 2.0f * flat.radius; // no room between the caps
    EXPECT_FALSE(world.CreateCharacter(flat).HasValue());
    CharacterDesc thin = desc;
    thin.radius = 0.0f;
    EXPECT_FALSE(world.CreateCharacter(thin).HasValue());
    CharacterDesc cliff = desc;
    cliff.maxSlopeDeg = kMaxSlopeLimitDeg;
    EXPECT_FALSE(world.CreateCharacter(cliff).HasValue());
    CharacterDesc sinking = desc;
    sinking.stepHeight = -0.1f;
    EXPECT_FALSE(world.CreateCharacter(sinking).HasValue());
    CharacterDesc floating = desc;
    floating.stickToFloor = -0.1f;
    EXPECT_FALSE(world.CreateCharacter(floating).HasValue());
    EXPECT_EQ(world.Stats().characters, 1u);
}

#if RUNIC_ASSERTS_ENABLED

TEST(CharactersDeathTest, NeedALayerOfTheWorldAndALiveHandle) {
    PhysicsWorld world;
    CharacterDesc desc;
    EXPECT_DEATH((void)world.CreateCharacter(desc), "names a collision layer");
    EXPECT_DEATH((void)world.Character(CharacterHandle()), "names no character");
}

#endif // RUNIC_ASSERTS_ENABLED
