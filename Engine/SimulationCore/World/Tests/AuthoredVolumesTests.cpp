// WORLD-2: authored volumes. A point inside a cave is reported inside it, exactly; the kind picks
// the backend, and a static rule area never needs a Physics body.

#include "AuthoredVolumes.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <random>
#include <vector>

using namespace Runic;
using namespace Runic::World;
using Core::Quat;
using Core::Vec3;
using Core::WorldPosition;

namespace {

const WorldPosition kFarAway{6'000'000.0, 0.0, -5'000'000.0};

WorldPosition At(double x, double y, double z) {
    return {kFarAway.x + x, kFarAway.y + y, kFarAway.z + z};
}

// A turn about the up axis.
Quat AboutUp(float radians) {
    return {std::cos(radians / 2.0f), 0.0f, std::sin(radians / 2.0f), 0.0f};
}

struct Kinds {
    VolumeKindId cave;
    VolumeKindId questArea;
    VolumeKindId deck;
    VolumeKindId wall;
};

Kinds AddKinds(VolumeSet &volumes) {
    Kinds kinds;
    kinds.cave = volumes.AddKind({"engine:test_cave"}).Value();
    kinds.questArea = volumes.AddKind({"engine:test_quest_area"}).Value();
    kinds.deck = volumes.AddKind({"engine:test_deck", false, true}).Value();
    kinds.wall = volumes.AddKind({"engine:test_wall", true, false}).Value();
    return kinds;
}

// A cave 200 m under the surface: a tunnel turned 30° (a long box), a round chamber at its end
// and a shaft (a cylinder) climbing from the chamber.
VolumeDesc Cave(VolumeKindId kind) {
    VolumeDesc cave;
    cave.kind = kind;
    cave.origin = At(100.0, -200.0, 50.0);
    cave.parts.push_back(BoxPart(Vec3(60.0f, 4.0f, 3.0f), Vec3(0.0f), AboutUp(0.5236f)));
    cave.parts.push_back(SpherePart(15.0f, Vec3(52.0f, 0.0f, -30.0f)));
    cave.parts.push_back(CylinderPart(3.0f, 40.0f, Vec3(52.0f, 50.0f, -30.0f)));
    return cave;
}

} // namespace

TEST(AuthoredVolumes, APointInsideACaveIsReportedInside) {
    VolumeSet volumes;
    const Kinds kinds = AddKinds(volumes);
    const VolumeId cave = volumes.Add(Cave(kinds.cave)).Value();
    std::vector<VolumeId> at;
    const auto inside = [&](WorldPosition point) {
        at.clear();
        volumes.VolumesAt(point, at);
        EXPECT_EQ(at.size() == 1 && at[0] == cave, volumes.IsInside(cave, point));
        return volumes.IsInside(cave, point);
    };
    // Along the tunnel, turned 30° about up: 50 m along its length is (43.3, 0, -25).
    EXPECT_TRUE(inside(At(100.0 + 43.3, -200.0, 50.0 - 25.0)));
    EXPECT_TRUE(inside(At(100.0 - 43.3, -197.0, 50.0 + 25.0)));
    // The same 50 m along the world's x axis is rock beside the tunnel.
    EXPECT_FALSE(inside(At(150.0, -200.0, 50.0)));
    // In the chamber, and in the shaft above it.
    EXPECT_TRUE(inside(At(152.0, -200.0, 20.0)));
    EXPECT_TRUE(inside(At(152.0, -189.0, 21.0)));
    EXPECT_TRUE(inside(At(152.0, -115.0, 22.0)));
    EXPECT_FALSE(inside(At(152.0, -115.0, 24.0))); // beside the shaft
    // On the surface above the cave: outside, though the cells are the same columns.
    EXPECT_FALSE(inside(At(100.0, 0.0, 50.0)));
    EXPECT_FALSE(inside(At(152.0, -105.0, 20.0))); // above the shaft's top
    // The tunnel's wall, exactly: 4 m up from its middle is on it, a millimetre more is rock.
    EXPECT_TRUE(inside(At(100.0, -196.0, 50.0)));
    EXPECT_FALSE(inside(At(100.0, -195.999, 50.0)));
}

TEST(AuthoredVolumes, AStaticRuleAreaCreatesNoPhysicsBody) {
    VolumeSet volumes;
    const Kinds kinds = AddKinds(volumes);
    EXPECT_EQ(volumes.DescribeKind(kinds.cave).backend, VolumeBackend::SpatialIndex);
    EXPECT_EQ(volumes.DescribeKind(kinds.questArea).backend, VolumeBackend::SpatialIndex);
    EXPECT_EQ(volumes.DescribeKind(kinds.deck).backend, VolumeBackend::SensorBody);
    EXPECT_EQ(volumes.DescribeKind(kinds.wall).backend, VolumeBackend::StaticBody);

    VolumeDesc area;
    area.kind = kinds.questArea;
    area.origin = At(0.0, 0.0, 0.0);
    area.parts.push_back(CylinderPart(40.0f, 20.0f));
    const VolumeId quest = volumes.Add(area).Value();
    volumes.Add(Cave(kinds.cave)).Value();
    std::vector<VolumeId> bodies;
    volumes.PhysicsVolumes(bodies);
    EXPECT_TRUE(bodies.empty()); // two rule areas, no body
    EXPECT_EQ(volumes.Stats().indexed, 2u);
    EXPECT_EQ(volumes.Stats().physics, 0u);
    std::vector<VolumeId> at;
    volumes.VolumesAt(At(10.0, 5.0, -10.0), at);
    EXPECT_EQ(at, std::vector<VolumeId>{quest});

    // A ship's deck follows its ship as a sensor, and a wall is solid: both need a body, and
    // neither is in World's index.
    VolumeDesc deck;
    deck.kind = kinds.deck;
    deck.origin = At(5.0, 2.0, 5.0);
    deck.parts.push_back(BoxPart(Vec3(10.0f, 1.0f, 4.0f)));
    const VolumeId onDeck = volumes.Add(deck).Value();
    VolumeDesc wall;
    wall.kind = kinds.wall;
    wall.origin = At(-20.0, 0.0, 0.0);
    wall.parts.push_back(BoxPart(Vec3(0.5f, 3.0f, 10.0f)));
    const VolumeId solid = volumes.Add(wall).Value();
    volumes.PhysicsVolumes(bodies);
    EXPECT_EQ(bodies, (std::vector<VolumeId>{onDeck, solid}));
    at.clear();
    volumes.VolumesAt(At(5.0, 2.0, 5.0), at); // on the deck and in the quest area
    EXPECT_EQ(at, std::vector<VolumeId>{quest});
    EXPECT_TRUE(volumes.IsInside(onDeck, At(5.0, 2.0, 5.0)));
    EXPECT_EQ(volumes.Stats().indexed, 2u);
}

TEST(AuthoredVolumes, PointQueriesMatchBruteForce) {
    std::mt19937_64 random(11);
    VolumeSet volumes;
    const Kinds kinds = AddKinds(volumes);
    std::uniform_real_distribution<double> ground(-600.0, 600.0);
    std::uniform_real_distribution<double> height(-300.0, 300.0);
    std::uniform_real_distribution<float> size(1.0f, 60.0f);
    std::uniform_real_distribution<float> angle(0.0f, 6.28f);
    std::vector<VolumeId> indexed;
    for (int i = 0; i < 500; ++i) {
        VolumeDesc desc;
        desc.kind = i % 7 == 0 ? kinds.deck : kinds.questArea;
        desc.origin = At(ground(random), height(random), ground(random));
        desc.rotation = AboutUp(angle(random));
        desc.parts.push_back(BoxPart(Vec3(size(random), size(random), size(random)), Vec3(0.0f),
                                     AboutUp(angle(random))));
        if (i % 2 == 0) {
            desc.parts.push_back(SpherePart(size(random), Vec3(size(random), 0.0f, 0.0f)));
        }
        if (i % 3 == 0) {
            desc.parts.push_back(
                CylinderPart(size(random), size(random), Vec3(0.0f, size(random), 0.0f)));
        }
        const VolumeId volume = volumes.Add(desc).Value();
        if (desc.kind == kinds.questArea) {
            indexed.push_back(volume);
        }
    }
    std::vector<VolumeId> at;
    std::size_t hits = 0;
    for (int i = 0; i < 5'000; ++i) {
        const WorldPosition point = At(ground(random), height(random), ground(random));
        std::vector<VolumeId> expected;
        for (const VolumeId volume : indexed) {
            if (volumes.IsInside(volume, point)) {
                expected.push_back(volume);
            }
        }
        at.clear();
        volumes.VolumesAt(point, at);
        ASSERT_EQ(at, expected) << "point " << i;
        hits += at.size();
    }
    EXPECT_GT(hits, 300u);
}

TEST(AuthoredVolumes, AnIndivisibleVolumeNamesItsCells) {
    VolumeSet volumes; // 64 m cells
    const Kinds kinds = AddKinds(volumes);
    VolumeDesc island;
    island.kind = kinds.questArea;
    island.origin = At(32.0, 1'000.0, 32.0);
    island.parts.push_back(CylinderPart(150.0f, 40.0f));
    island.indivisible = true;
    island.owner = 77;
    const VolumeId id = volumes.Add(island).Value();
    const VolumeInfo info = volumes.Describe(id);
    EXPECT_TRUE(info.indivisible);
    EXPECT_EQ(info.owner, 77u);
    EXPECT_EQ(info.parts, 1u);
    const CellRange cells = volumes.CellsOf(id);
    // 300 m across from x = 32 - 150 to 32 + 150: cells -2 to 2 past the far-away column; 80 m
    // tall from 960 to 1,040: cells 15 and 16.
    const CellCoord middle = CoordinateSpace{}.CellOf(At(0.0, 0.0, 0.0));
    EXPECT_EQ(cells.min.x - middle.x, -2);
    EXPECT_EQ(cells.max.x - middle.x, 2);
    EXPECT_EQ(cells.min.y, 15);
    EXPECT_EQ(cells.max.y, 16);
    EXPECT_EQ(cells.Count(), 50u);
    EXPECT_EQ(volumes.Stats().indivisible, 1u);
}

TEST(AuthoredVolumes, AVolumeThatFollowsAnEntityMoves) {
    VolumeSet volumes;
    const Kinds kinds = AddKinds(volumes);
    VolumeDesc deck;
    deck.kind = kinds.deck;
    deck.origin = At(0.0, 0.0, 0.0);
    deck.parts.push_back(BoxPart(Vec3(10.0f, 1.0f, 3.0f)));
    const VolumeId id = volumes.Add(deck).Value();
    EXPECT_TRUE(volumes.IsInside(id, At(9.0, 0.0, 0.0)));
    // The ship sails 1 km and turns a quarter: the deck goes with it.
    volumes.SetPose(id, At(1'000.0, 0.0, 0.0), AboutUp(1.5708f));
    EXPECT_FALSE(volumes.IsInside(id, At(9.0, 0.0, 0.0)));
    EXPECT_FALSE(volumes.IsInside(id, At(1'009.0, 0.0, 0.0))); // its length now runs along z
    EXPECT_TRUE(volumes.IsInside(id, At(1'000.0, 0.0, 9.0)));
    EXPECT_EQ(volumes.Describe(id).origin, At(1'000.0, 0.0, 0.0));
}

TEST(AuthoredVolumes, KindsAndVolumesAreChecked) {
    VolumeSet volumes;
    const Kinds kinds = AddKinds(volumes);
    EXPECT_FALSE(volumes.AddKind({"test_cave"}));                           // no namespace
    EXPECT_FALSE(volumes.AddKind({"engine:test_cave"}));                    // taken
    EXPECT_FALSE(volumes.AddKind({"engine:test_moving_wall", true, true})); // solid and moving
    EXPECT_EQ(volumes.FindKind("engine:test_cave"), kinds.cave);
    EXPECT_FALSE(volumes.FindKind("engine:test_nothing").IsValid());
    EXPECT_EQ(volumes.DescribeKind(kinds.wall).name, "engine:test_wall");

    VolumeDesc desc;
    desc.kind = VolumeKindId(99);
    desc.parts.push_back(SpherePart(1.0f));
    EXPECT_FALSE(volumes.Add(desc)); // a kind the set does not have
    desc.kind = kinds.cave;
    desc.parts.clear();
    EXPECT_FALSE(volumes.Add(desc)); // no parts
    desc.parts.push_back(BoxPart(Vec3(1.0f, 0.001f, 1.0f)));
    EXPECT_FALSE(volumes.Add(desc)); // a part thinner than a centimetre
    desc.parts[0] = CylinderPart(1.0f, 0.0f);
    EXPECT_FALSE(volumes.Add(desc));
    EXPECT_EQ(volumes.Stats().volumes, 0u);

    // A removed volume's ID never names the one that takes its place.
    desc.parts[0] = SpherePart(1.0f);
    const VolumeId first = volumes.Add(desc).Value();
    volumes.Remove(first);
    const VolumeId second = volumes.Add(desc).Value();
    EXPECT_NE(first, second);
    EXPECT_FALSE(volumes.IsVolume(first));
    EXPECT_TRUE(volumes.IsVolume(second));
    std::vector<VolumeId> all;
    volumes.Volumes(all);
    EXPECT_EQ(all, std::vector<VolumeId>{second});
}

#if RUNIC_ASSERTS_ENABLED

TEST(AuthoredVolumesDeathTest, AStaticVolumeNeverMoves) {
    VolumeSet volumes;
    const Kinds kinds = AddKinds(volumes);
    const VolumeId cave = volumes.Add(Cave(kinds.cave)).Value();
    EXPECT_DEATH(volumes.SetPose(cave, At(1.0, 0.0, 0.0), kNoRotation), "does not move");
    volumes.Remove(cave);
    EXPECT_DEATH(volumes.Describe(cave), "not a volume");
}

#endif
