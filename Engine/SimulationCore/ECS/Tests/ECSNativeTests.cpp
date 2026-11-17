#include "ECSNative.hpp"

#include <gtest/gtest.h>

#include "EntityWorld.hpp"

using namespace Runic::ECS;

namespace {

struct Marker {
    int value = 0;
};

} // namespace

// The opt-in door: Flecs itself, reached through Runic::ECSNative, on the same world.
TEST(ECSNative, TheNativeDoorReachesTheSameWorld) {
    EntityWorld world;
    world.RegisterComponent<Marker>("engine:test_marker");
    const Entity entity = world.Create();
    world.Set(entity, Marker{42});

    ecs_world_t *native = Native::WorldOf(world);
    ASSERT_NE(native, nullptr);
    const ecs_entity_t id = Native::ToNative(entity);
    EXPECT_TRUE(ecs_is_alive(native, id));
    const ecs_id_t marker = Native::ComponentOf<Marker>(world);
    ASSERT_TRUE(ecs_has_id(native, id, marker));
    EXPECT_EQ(static_cast<const Marker *>(ecs_get_id(native, id, marker))->value, 42);
    EXPECT_STREQ(ecs_get_name(native, marker), "engine:test_marker");

    // A change made through the door is seen through the Runic API.
    const Marker changed{7};
    ecs_set_id(native, id, marker, sizeof(Marker), &changed);
    EXPECT_EQ(world.Get<Marker>(entity).value, 7);
    EXPECT_EQ(Native::FromNative(id), entity);
}

TEST(ECSNative, AnEntityMadeThroughTheDoorIsNotOneOfTheWorldsEntities) {
    EntityWorld world;
    const Entity made = world.Create();
    const ecs_entity_t raw = ecs_new(Native::WorldOf(world));

    EXPECT_TRUE(world.IsAlive(Native::FromNative(raw))); // alive in the library
    EXPECT_EQ(world.EntityCount(), 1u);                  // but not counted or listed
    int listed = 0;
    world.ForEachEntity([&](Entity entity) {
        EXPECT_EQ(entity, made);
        ++listed;
    });
    EXPECT_EQ(listed, 1);
}
