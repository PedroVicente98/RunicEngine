#include "WorldRegistry.hpp"

#include <gtest/gtest.h>

#include <string>

using namespace Runic;
using namespace Runic::World;

namespace {

constexpr Core::WorldId kMainWorld{1};
constexpr Core::WorldId kOtherWorld{2};
constexpr Core::WorldId kUnknownWorld{99};

// A registry with two worlds that divide space differently.
struct Worlds {
    Worlds() {
        WorldDescription main;
        main.name = "engine:test_main";
        EXPECT_TRUE(registry.Register(kMainWorld, main)); // the default 64 m and 512 m

        WorldDescription other;
        other.name = "engine:test_other";
        other.space.origin = Core::WorldPosition{1'000.0, -500.0, 1'000.0};
        other.space.cellSizeM = 100;
        other.space.cellsPerSector = 4;
        EXPECT_TRUE(registry.Register(kOtherWorld, other));
    }

    WorldRegistry registry;
};

} // namespace

TEST(WorldRegistry, GivesEachWorldItsCoordinateSpace) {
    const Worlds worlds;
    EXPECT_TRUE(worlds.registry.Knows(kMainWorld));
    EXPECT_TRUE(worlds.registry.Knows(kOtherWorld));
    ASSERT_EQ(worlds.registry.Count(), 2u);

    const WorldInfo main = worlds.registry.Describe(0);
    EXPECT_EQ(main.id, kMainWorld);
    EXPECT_EQ(main.name, "engine:test_main");
    EXPECT_EQ(main.space.cellSizeM, kDefaultCellSizeM);
    EXPECT_EQ(main.space.origin, (Core::WorldPosition{0.0, 0.0, 0.0}));
    const Core::Result<CoordinateSpace> other = worlds.registry.SpaceOf(kOtherWorld);
    ASSERT_TRUE(other);
    EXPECT_EQ(other.Value().cellSizeM, 100u);
    EXPECT_EQ(other.Value().cellsPerSector, 4u);
    EXPECT_EQ(other.Value().origin, (Core::WorldPosition{1'000.0, -500.0, 1'000.0}));
}

TEST(WorldRegistry, AWorldIdItDoesNotKnowIsRejected) {
    const Worlds worlds;
    const Core::WorldPosition place{10.0, 20.0, 30.0};
    const CellKey cell{kUnknownWorld, {0, 0, 0}};
    EXPECT_FALSE(worlds.registry.Knows(kUnknownWorld));
    EXPECT_FALSE(worlds.registry.Knows(Core::WorldId())); // the invalid ID is not a world either

    const std::string expected = "world 99 is not registered";
    const Core::Result<CoordinateSpace> space = worlds.registry.SpaceOf(kUnknownWorld);
    ASSERT_FALSE(space);
    EXPECT_EQ(space.GetError().message, expected);
    const Core::Result<CellKey> cellAt = worlds.registry.CellAt(kUnknownWorld, place);
    ASSERT_FALSE(cellAt);
    EXPECT_EQ(cellAt.GetError().message, expected);
    const Core::Result<SectorKey> sectorAt = worlds.registry.SectorAt(kUnknownWorld, place);
    ASSERT_FALSE(sectorAt);
    EXPECT_EQ(sectorAt.GetError().message, expected);
    const Core::Result<Core::WorldPosition> min = worlds.registry.CellMin(cell);
    ASSERT_FALSE(min);
    EXPECT_EQ(min.GetError().message, expected);
    const Core::Result<Core::WorldPosition> center = worlds.registry.CellCenter(cell);
    ASSERT_FALSE(center);
    EXPECT_EQ(center.GetError().message, expected);
    const Core::Result<SectorKey> sector = worlds.registry.SectorOf(cell);
    ASSERT_FALSE(sector);
    EXPECT_EQ(sector.GetError().message, expected);
    EXPECT_FALSE(worlds.registry.SpaceOf(Core::WorldId()));
}

TEST(WorldRegistry, ABadRegistrationIsRefusedAndChangesNothing) {
    Worlds worlds;
    WorldDescription description;
    description.name = "engine:test_new";

    Core::Result<void> result = worlds.registry.Register(Core::WorldId(), description);
    ASSERT_FALSE(result);
    EXPECT_EQ(result.GetError().message, "world 'engine:test_new' has no valid WorldId");

    result = worlds.registry.Register(kMainWorld, description);
    ASSERT_FALSE(result);
    EXPECT_EQ(result.GetError().message, "world 1 is already registered as 'engine:test_main'");

    description.name = "engine:test_other";
    result = worlds.registry.Register(Core::WorldId(3), description);
    ASSERT_FALSE(result);
    EXPECT_EQ(result.GetError().message, "the world name 'engine:test_other' is already taken");

    description.name = "";
    result = worlds.registry.Register(Core::WorldId(3), description);
    ASSERT_FALSE(result);
    EXPECT_EQ(result.GetError().message, "world 3 has no name");

    description.name = "engine:test_new";
    description.space.cellSizeM = 0;
    result = worlds.registry.Register(Core::WorldId(3), description);
    ASSERT_FALSE(result);
    EXPECT_EQ(result.GetError().message,
              "world 'engine:test_new' has a cell or sector size of zero");
    description.space.cellSizeM = 64;
    description.space.cellsPerSector = 0;
    EXPECT_FALSE(worlds.registry.Register(Core::WorldId(3), description));

    EXPECT_EQ(worlds.registry.Count(), 2u);
    EXPECT_FALSE(worlds.registry.Knows(Core::WorldId(3)));
    description.space.cellsPerSector = 8;
    EXPECT_TRUE(worlds.registry.Register(Core::WorldId(3), description));
    EXPECT_TRUE(worlds.registry.Knows(Core::WorldId(3)));
}

TEST(WorldRegistry, TheSamePositionIsAnotherCellInAnotherWorld) {
    const Worlds worlds;
    const Core::WorldPosition place{6'000'130.0, -3'000.0, -250.0};

    const Core::Result<CellKey> inMain = worlds.registry.CellAt(kMainWorld, place);
    const Core::Result<CellKey> inOther = worlds.registry.CellAt(kOtherWorld, place);
    ASSERT_TRUE(inMain);
    ASSERT_TRUE(inOther);
    EXPECT_EQ(inMain.Value(), (CellKey{kMainWorld, {93'752, -47, -4}}));
    EXPECT_EQ(inOther.Value(), (CellKey{kOtherWorld, {59'991, -25, -13}}));
    EXPECT_NE(inMain.Value(), inOther.Value());
}

TEST(WorldRegistry, ACellKeyRoundTripsThroughTheRegistry) {
    const Worlds worlds;
    for (const Core::WorldId world : {kMainWorld, kOtherWorld}) {
        for (const std::int32_t y : {-2'000'000, -300, 0, 12, 2'000'000}) { // any altitude or depth
            const CellKey key{world, {-93'750, y, 117'000}};
            const Core::Result<Core::WorldPosition> min = worlds.registry.CellMin(key);
            const Core::Result<Core::WorldPosition> center = worlds.registry.CellCenter(key);
            ASSERT_TRUE(min);
            ASSERT_TRUE(center);
            const Core::Result<CellKey> fromMin = worlds.registry.CellAt(world, min.Value());
            const Core::Result<CellKey> fromCenter = worlds.registry.CellAt(world, center.Value());
            ASSERT_TRUE(fromMin);
            EXPECT_EQ(fromMin.Value(), key) << "y = " << y;
            EXPECT_EQ(fromCenter.Value(), key) << "y = " << y;

            const Core::Result<SectorKey> sector = worlds.registry.SectorOf(key);
            const Core::Result<SectorKey> sectorAt =
                worlds.registry.SectorAt(world, center.Value());
            ASSERT_TRUE(sector);
            ASSERT_TRUE(sectorAt);
            EXPECT_EQ(sector.Value(), sectorAt.Value());
            EXPECT_EQ(sector.Value().world, world);
        }
    }
}

TEST(WorldRegistry, AnInstanceIsACopyOfAWorldAndSharesItsSpace) {
    const Worlds worlds;
    const WorldInstance shared{kMainWorld};
    const WorldInstance dungeonRun{kMainWorld, Core::InstanceId(41)};
    const WorldInstance anotherRun{kMainWorld, Core::InstanceId(42)};
    EXPECT_TRUE(shared.IsShared());
    EXPECT_EQ(shared.instance, kSharedInstance);
    EXPECT_FALSE(dungeonRun.IsShared());
    EXPECT_NE(dungeonRun, anotherRun); // two runs of the same place are two instances
    EXPECT_NE(dungeonRun, shared);

    // Both runs use the coordinate space of their world.
    const Core::Result<CoordinateSpace> space = worlds.registry.SpaceOf(dungeonRun.world);
    ASSERT_TRUE(space);
    EXPECT_EQ(space.Value(), worlds.registry.SpaceOf(anotherRun.world).Value());
}
