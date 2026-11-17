#include "WorldComponents.hpp"

#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "CoordinateSpace.hpp"
#include "EntityWorld.hpp"

using namespace Runic;
using namespace Runic::World;

// The runnable result of slice 4: entities with 64-bit positions are created and queried.
TEST(WorldComponents, EntitiesWith64BitPositionsAreCreatedAndQueried) {
    ECS::EntityWorld world;
    RegisterWorldComponents(world);
    RegisterWorldComponents(world); // again: nothing happens
    const auto placed = world.MakeQuery<WorldTransform>("engine:test_placed");

    // A row of entities one centimetre apart, six thousand kilometres from the origin and three
    // kilometres down: a 32-bit float could not tell them apart there (its step is 50 cm).
    const Core::WorldPosition start{6'000'000.0, -3'000.0, -6'000'000.0};
    constexpr double kOneCentimetre = 0.01;
    std::vector<ECS::Entity> entities;
    for (int index = 0; index < 1'000; ++index) {
        const ECS::Entity entity = world.Create();
        WorldTransform transform;
        transform.position = start + Core::WorldDelta{kOneCentimetre * index, 0.0, 0.0};
        world.Set(entity, transform);
        entities.push_back(entity);
    }
    EXPECT_EQ(placed.Count(), 1'000u);

    // A system moves everyone one centimetre up, in place.
    placed.Each([&](ECS::Entity, WorldTransform &transform) {
        transform.position = transform.position + Core::WorldDelta{0.0, kOneCentimetre, 0.0};
    });

    for (int index = 0; index < 1'000; ++index) {
        const WorldTransform &transform =
            world.Get<WorldTransform>(entities[static_cast<std::size_t>(index)]);
        ASSERT_DOUBLE_EQ(transform.position.x, 6'000'000.0 + kOneCentimetre * index);
        ASSERT_DOUBLE_EQ(transform.position.y, -3'000.0 + kOneCentimetre);
        ASSERT_DOUBLE_EQ(transform.position.z, -6'000'000.0);
        ASSERT_EQ(transform.rotation, kNoRotation);
        if (index > 0) {
            const WorldTransform &previous =
                world.Get<WorldTransform>(entities[static_cast<std::size_t>(index) - 1]);
            ASSERT_NE(transform.position.x, previous.position.x); // still distinct
        }
    }

    // And each is found in the cell its position says: the 10 m row lies inside one 64 m cell.
    const CoordinateSpace space;
    const CellCoord first = space.CellOf(world.Get<WorldTransform>(entities.front()).position);
    const CellCoord last = space.CellOf(world.Get<WorldTransform>(entities.back()).position);
    EXPECT_EQ(first, (CellCoord{93'750, -47, -93'750}));
    EXPECT_EQ(last, first);
}

TEST(WorldComponents, ARegionIsAnEntityWithItsWorldAndItsFrame) {
    ECS::EntityWorld world;
    RegisterWorldComponents(world);
    const auto regions = world.MakeQuery<const Region>("engine:test_regions");

    const ECS::Entity harbor = world.Create();
    world.Set(harbor, Region{Core::WorldId(1), RegionFrame{{2'500'000.0, 0.0, -1'250'000.0}}});
    const ECS::Entity prop = world.Create();

    // Content authored in the region's own coordinates is placed through the region's frame.
    const Region &region = world.Get<Region>(harbor);
    WorldTransform transform;
    transform.position = region.frame.ToWorld(Core::Vec3(12.5f, 3.0f, -40.25f));
    world.Set(prop, transform);

    EXPECT_EQ(regions.Count(), 1u);
    EXPECT_EQ(region.world, Core::WorldId(1));
    EXPECT_DOUBLE_EQ(world.Get<WorldTransform>(prop).position.x, 2'500'012.5);
    EXPECT_DOUBLE_EQ(world.Get<WorldTransform>(prop).position.z, -1'250'040.25);
    EXPECT_EQ(region.frame.ToRegionLocal(world.Get<WorldTransform>(prop).position),
              Core::Vec3(12.5f, 3.0f, -40.25f));
}

TEST(WorldComponents, ToolsSeeWhereAnEntityIs) {
    ECS::EntityWorld world;
    RegisterWorldComponents(world);
    const ECS::Entity entity = world.Create();
    WorldTransform transform;
    transform.position = Core::WorldPosition{6'000'000.125, -42.5, 7.0};
    world.Set(entity, transform);
    world.Set(entity, Region{Core::WorldId(3), RegionFrame{{100.0, 0.0, -100.0}}});

    std::vector<std::string> shown;
    world.ForEachComponentOf(entity, [&](const ECS::ComponentInfo &info, const void *component) {
        ASSERT_NE(info.format, nullptr);
        std::string text;
        info.format(component, text);
        shown.push_back(std::string(info.name) + ": " + text);
    });
    ASSERT_EQ(shown.size(), 2u);
    EXPECT_EQ(shown[0], "engine:world_transform: (6000000.12, -42.50, 7.00) m");
    EXPECT_EQ(shown[1], "engine:region: world 3, origin (100.00, 0.00, -100.00) m");
    EXPECT_EQ(world.DescribeComponent(0).name, kWorldTransformName);
    EXPECT_EQ(world.DescribeComponent(1).name, kRegionName);
}
