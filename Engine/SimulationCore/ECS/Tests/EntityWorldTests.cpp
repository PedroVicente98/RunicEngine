#include "EntityWorld.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <set>
#include <string>
#include <type_traits>
#include <unordered_set>
#include <vector>

using namespace Runic::ECS;

namespace {

struct Position {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

struct Velocity {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct Health {
    std::int32_t current = 100;
    std::int32_t max = 100;
};

// A component that owns memory and counts its living instances.
struct Inventory {
    Inventory() { ++s_Alive; }
    Inventory(const Inventory &other) : items(other.items) { ++s_Alive; }
    Inventory(Inventory &&other) noexcept : items(std::move(other.items)) { ++s_Alive; }
    Inventory &operator=(const Inventory &) = default;
    Inventory &operator=(Inventory &&) noexcept = default;
    ~Inventory() { --s_Alive; }

    std::vector<std::string> items;

    static inline int s_Alive = 0;
};

void RegisterAll(EntityWorld &world) {
    world.RegisterComponent<Position>("engine:test_position");
    world.RegisterComponent<Velocity>("engine:test_velocity");
    world.RegisterComponent<Health>("engine:test_health");
}

} // namespace

TEST(EntityWorld, CreatesAndDestroysEntities) {
    EntityWorld world;
    EXPECT_EQ(world.EntityCount(), 0u);

    const Entity first = world.Create();
    const Entity second = world.Create();
    EXPECT_FALSE(first.IsNull());
    EXPECT_NE(first, second);
    EXPECT_TRUE(world.IsAlive(first));
    EXPECT_TRUE(world.IsAlive(second));
    EXPECT_EQ(world.EntityCount(), 2u);

    world.Destroy(first);
    EXPECT_FALSE(world.IsAlive(first));
    EXPECT_TRUE(world.IsAlive(second));
    EXPECT_EQ(world.EntityCount(), 1u);

    world.Destroy(first); // already gone: ignored
    world.Destroy(Entity());
    EXPECT_EQ(world.EntityCount(), 1u);
}

TEST(EntityWorld, TheNullEntityIsNeverAlive) {
    const EntityWorld world;
    const Entity null;
    EXPECT_TRUE(null.IsNull());
    EXPECT_FALSE(static_cast<bool>(null));
    EXPECT_EQ(null.Raw(), 0u);
    EXPECT_FALSE(world.IsAlive(null));
    EXPECT_EQ(Entity::FromRaw(null.Raw()), null);
}

TEST(EntityWorld, ADestroyedEntitysHandleNeverComesBackToLife) {
    EntityWorld world;
    std::vector<Entity> stale;
    for (int round = 0; round < 100; ++round) {
        const Entity entity = world.Create();
        world.Destroy(entity);
        stale.push_back(entity);
    }
    // The slots are reused by new entities, but an old handle stays dead and no new handle
    // equals one.
    std::unordered_set<Entity> seen(stale.begin(), stale.end());
    for (int round = 0; round < 100; ++round) {
        const Entity entity = world.Create();
        EXPECT_TRUE(seen.insert(entity).second) << "a handle was handed out twice";
    }
    for (const Entity old : stale) {
        EXPECT_FALSE(world.IsAlive(old));
    }
    EXPECT_EQ(world.EntityCount(), 100u);
}

TEST(EntityWorld, SetsReadsAndRemovesComponents) {
    EntityWorld world;
    RegisterAll(world);
    const Entity entity = world.Create();
    EXPECT_FALSE(world.Has<Position>(entity));
    EXPECT_EQ(world.TryGet<Position>(entity), nullptr);

    world.Set(entity, Position{6'000'000.5, -120.25, 3.0});
    world.Set(entity, Health{70, 100});
    ASSERT_TRUE(world.Has<Position>(entity));
    EXPECT_DOUBLE_EQ(world.Get<Position>(entity).x, 6'000'000.5);
    EXPECT_DOUBLE_EQ(world.Get<Position>(entity).y, -120.25);
    EXPECT_EQ(world.Get<Health>(entity).current, 70);
    EXPECT_FALSE(world.Has<Velocity>(entity));

    world.Get<Health>(entity).current -= 25;    // a value changes in place
    world.Set(entity, Position{1.0, 2.0, 3.0}); // and Set overwrites
    const EntityWorld &readOnly = world;
    EXPECT_EQ(readOnly.Get<Health>(entity).current, 45);
    EXPECT_DOUBLE_EQ(readOnly.TryGet<Position>(entity)->x, 1.0);

    world.Remove<Position>(entity);
    world.Remove<Velocity>(entity); // not there: nothing happens
    EXPECT_FALSE(world.Has<Position>(entity));
    EXPECT_TRUE(world.Has<Health>(entity));
    EXPECT_TRUE(world.IsAlive(entity));
}

TEST(EntityWorld, ComponentsAreRegisteredOncePerWorldUnderANamespacedName) {
    EntityWorld world;
    EXPECT_FALSE(world.IsRegistered<Position>());
    EXPECT_EQ(world.ComponentCount(), 0u);

    ComponentOptions options;
    options.description = "Where a test entity is";
    options.format = [](const void *component, std::string &out) {
        const auto &position = *static_cast<const Position *>(component);
        out = std::to_string(static_cast<int>(position.x)) + ", " +
              std::to_string(static_cast<int>(position.y));
    };
    world.RegisterComponent<Position>("engine:test_position", options);
    world.RegisterComponent<Health>("engine:test_health");
    world.RegisterComponent<Position>("engine:test_position", options); // again: nothing happens

    EXPECT_TRUE(world.IsRegistered<Position>());
    EXPECT_TRUE(world.IsRegistered<const Position>());
    EXPECT_FALSE(world.IsRegistered<Velocity>());
    ASSERT_EQ(world.ComponentCount(), 2u);
    const ComponentInfo position = world.DescribeComponent(0);
    EXPECT_EQ(position.name, "engine:test_position");
    EXPECT_EQ(position.description, "Where a test entity is");
    EXPECT_EQ(position.size, sizeof(Position));
    EXPECT_EQ(position.alignment, alignof(Position));
    ASSERT_NE(position.format, nullptr);
    std::string text;
    const Position value{4.0, 2.0, 0.0};
    position.format(&value, text);
    EXPECT_EQ(text, "4, 2");
    EXPECT_EQ(world.DescribeComponent(1).name, "engine:test_health");
    EXPECT_EQ(world.DescribeComponent(1).format, nullptr);
}

TEST(EntityWorld, AComponentThatOwnsMemoryIsBuiltMovedAndDestroyedProperly) {
    Inventory::s_Alive = 0;
    {
        EntityWorld world;
        RegisterAll(world);
        world.RegisterComponent<Inventory>("engine:test_inventory");

        std::vector<Entity> entities;
        for (int index = 0; index < 50; ++index) {
            const Entity entity = world.Create();
            Inventory inventory;
            inventory.items = {"engine:test_item_" + std::to_string(index),
                               std::string(100, 'x')}; // too long for a small string
            world.Set(entity, inventory);
            entities.push_back(entity);
        }
        EXPECT_EQ(Inventory::s_Alive, 50);

        // Adding and removing other components moves the entities between storages; the
        // inventories go with them.
        for (std::size_t index = 0; index < entities.size(); ++index) {
            world.Set(entities[index], Position{static_cast<double>(index), 0.0, 0.0});
            if (index % 2 == 0) {
                world.Set(entities[index], Health{});
            }
        }
        for (std::size_t index = 0; index < entities.size(); index += 3) {
            world.Remove<Position>(entities[index]);
        }
        for (std::size_t index = 0; index < entities.size(); ++index) {
            const Inventory &inventory = world.Get<Inventory>(entities[index]);
            ASSERT_EQ(inventory.items.size(), 2u);
            EXPECT_EQ(inventory.items[0], "engine:test_item_" + std::to_string(index));
            EXPECT_EQ(inventory.items[1], std::string(100, 'x'));
        }
        EXPECT_EQ(Inventory::s_Alive, 50);

        world.Set(entities[0], Inventory{}); // overwriting keeps one instance
        EXPECT_TRUE(world.Get<Inventory>(entities[0]).items.empty());
        world.Remove<Inventory>(entities[1]);
        world.Destroy(entities[2]);
        EXPECT_EQ(Inventory::s_Alive, 48);
    }
    EXPECT_EQ(Inventory::s_Alive, 0); // the world destroyed the rest
}

TEST(EntityWorld, AQueryVisitsEveryEntityThatHasAllItsComponents) {
    EntityWorld world;
    RegisterAll(world);
    const auto moving = world.MakeQuery<Position, const Velocity>("engine:test_movement");
    EXPECT_TRUE(moving.IsValid());
    EXPECT_EQ(moving.Count(), 0u);

    std::set<Entity> expected;
    for (int index = 0; index < 300; ++index) {
        const Entity entity = world.Create();
        world.Set(entity, Position{static_cast<double>(index), 0.0, 0.0});
        if (index % 3 != 0) {
            world.Set(entity, Velocity{1.0f, 0.0f, 0.0f});
            expected.insert(entity);
        }
        if (index % 5 == 0) {
            world.Set(entity, Health{}); // another storage: the query crosses storages
        }
    }
    EXPECT_EQ(moving.Count(), 200u);

    std::set<Entity> visited;
    moving.Each([&](Entity entity, Position &position, const Velocity &velocity) {
        position.x += static_cast<double>(velocity.x);
        EXPECT_TRUE(visited.insert(entity).second) << "an entity was visited twice";
    });
    EXPECT_EQ(visited, expected);
    for (const Entity entity : expected) {
        const double x = world.Get<Position>(entity).x;
        EXPECT_DOUBLE_EQ(x - static_cast<double>(static_cast<int>(x)), 0.0);
    }
}

TEST(EntityWorld, AQueryMadeAtSetupSeesLaterEntitiesAndForgetsDestroyedOnes) {
    EntityWorld world;
    RegisterAll(world);
    const auto healthy = world.MakeQuery<const Health>("engine:test_health_scan");

    const Entity first = world.Create();
    world.Set(first, Health{10, 100});
    EXPECT_EQ(healthy.Count(), 1u);
    const Entity second = world.Create();
    world.Set(second, Health{20, 100});
    world.Set(second, Position{}); // a storage that did not exist when the query was made
    EXPECT_EQ(healthy.Count(), 2u);

    world.Destroy(first);
    world.Remove<Health>(second);
    EXPECT_EQ(healthy.Count(), 0u);
    int visits = 0;
    healthy.Each([&](Entity, const Health &) { ++visits; });
    EXPECT_EQ(visits, 0);
}

TEST(EntityWorld, ChunksAreContiguousColumnsInTheSameOrder) {
    EntityWorld world;
    RegisterAll(world);
    const auto query = world.MakeQuery<const Position, Velocity>("engine:test_chunks");
    for (int index = 0; index < 1'000; ++index) {
        const Entity entity = world.Create();
        world.Set(entity, Position{static_cast<double>(index), 0.0, 0.0});
        world.Set(entity, Velocity{static_cast<float>(index), 0.0f, 0.0f});
        if (index % 2 == 0) {
            world.Set(entity, Health{});
        }
    }

    std::uint32_t rows = 0;
    std::uint32_t chunks = 0;
    query.EachChunk([&](std::span<const Entity> entities, std::span<const Position> positions,
                        std::span<Velocity> velocities) {
        static_assert(std::is_same_v<decltype(positions), std::span<const Position>>);
        ASSERT_EQ(entities.size(), positions.size());
        ASSERT_EQ(entities.size(), velocities.size());
        for (std::size_t row = 0; row < entities.size(); ++row) {
            // Each row's values belong to the same entity.
            ASSERT_DOUBLE_EQ(positions[row].x, static_cast<double>(velocities[row].x));
            ASSERT_DOUBLE_EQ(world.Get<Position>(entities[row]).x, positions[row].x);
            velocities[row].y = 1.0f;
        }
        rows += static_cast<std::uint32_t>(entities.size());
        ++chunks;
    });
    EXPECT_EQ(rows, 1'000u);
    EXPECT_EQ(chunks, 2u); // two storages: with and without Health
    EXPECT_EQ(query.Count(), 1'000u);
}

TEST(EntityWorld, ChangesThatMoveEntitiesInsideEachApplyWhenItEnds) {
    EntityWorld world;
    RegisterAll(world);
    const auto query = world.MakeQuery<const Position>("engine:test_positions");
    std::vector<Entity> entities;
    for (int index = 0; index < 100; ++index) {
        const Entity entity = world.Create();
        world.Set(entity, Position{static_cast<double>(index), 0.0, 0.0});
        entities.push_back(entity);
    }

    int visited = 0;
    Entity created;
    EXPECT_FALSE(world.IsDeferred());
    query.Each([&](Entity entity, const Position &position) {
        EXPECT_TRUE(world.IsDeferred());
        ++visited;
        const int index = static_cast<int>(position.x);
        if (index % 2 == 0) {
            world.Destroy(entity);
            EXPECT_TRUE(world.IsAlive(entity)); // still there until the iteration ends
        } else {
            world.Set(entity, Velocity{1.0f, 0.0f, 0.0f});
            EXPECT_FALSE(world.Has<Velocity>(entity));
        }
        if (created.IsNull()) {
            created = world.Create();
            world.Set(created, Position{-1.0, 0.0, 0.0});
        }
    });
    EXPECT_FALSE(world.IsDeferred());

    EXPECT_EQ(visited, 100); // the new entity was not visited by the running iteration
    for (std::size_t index = 0; index < entities.size(); ++index) {
        if (index % 2 == 0) {
            EXPECT_FALSE(world.IsAlive(entities[index]));
        } else {
            EXPECT_TRUE(world.Has<Velocity>(entities[index]));
        }
    }
    EXPECT_TRUE(world.Has<Position>(created));
    EXPECT_EQ(query.Count(), 51u);
}

TEST(EntityWorld, ADeferScopeHoldsChangesUntilItEnds) {
    EntityWorld world;
    RegisterAll(world);
    const Entity entity = world.Create();
    world.Set(entity, Health{50, 100});
    {
        const EntityWorld::DeferScope outer(world);
        {
            const EntityWorld::DeferScope inner(world);
            world.Set(entity, Position{1.0, 2.0, 3.0});
            world.Remove<Health>(entity);
        }
        // The inner scope ended, the outer one still holds the changes.
        EXPECT_TRUE(world.IsDeferred());
        EXPECT_FALSE(world.Has<Position>(entity));
        EXPECT_TRUE(world.Has<Health>(entity));
    }
    EXPECT_FALSE(world.IsDeferred());
    EXPECT_DOUBLE_EQ(world.Get<Position>(entity).z, 3.0);
    EXPECT_FALSE(world.Has<Health>(entity));
}

TEST(EntityWorld, TwoWorldsAreIndependent) {
    EntityWorld first;
    EntityWorld second;
    RegisterAll(first);
    second.RegisterComponent<Health>("engine:test_health"); // another order, another set
    second.RegisterComponent<Position>("engine:test_position");

    const Entity a = first.Create();
    first.Set(a, Position{1.0, 0.0, 0.0});
    const Entity b = second.Create();
    second.Set(b, Position{2.0, 0.0, 0.0});
    second.Set(b, Health{5, 10});

    EXPECT_EQ(first.EntityCount(), 1u);
    EXPECT_EQ(second.EntityCount(), 1u);
    EXPECT_DOUBLE_EQ(first.Get<Position>(a).x, 1.0);
    EXPECT_DOUBLE_EQ(second.Get<Position>(b).x, 2.0);
    EXPECT_EQ(second.Get<Health>(b).current, 5);
    EXPECT_TRUE(first.IsRegistered<Velocity>());
    EXPECT_FALSE(second.IsRegistered<Velocity>());
    const auto inFirst = first.MakeQuery<const Position>("engine:test_a");
    const auto inSecond = second.MakeQuery<const Position, const Health>("engine:test_b");
    EXPECT_EQ(inFirst.Count(), 1u);
    EXPECT_EQ(inSecond.Count(), 1u);
}

TEST(EntityWorld, ToolsListEntitiesTheirComponentsAndTheQueries) {
    EntityWorld world;
    RegisterAll(world);
    const Entity plain = world.Create();
    const Entity full = world.Create();
    world.Set(full, Position{7.0, 8.0, 9.0});
    world.Set(full, Health{3, 4});
    world.MakeQuery<const Position>("engine:test_first");
    world.MakeQuery<Position, const Velocity, Health>("engine:test_second");

    std::set<Entity> entities;
    world.ForEachEntity([&](Entity entity) { entities.insert(entity); });
    EXPECT_EQ(entities, (std::set<Entity>{plain, full}));

    std::vector<std::string> names;
    world.ForEachComponentOf(full, [&](const ComponentInfo &info, const void *component) {
        names.emplace_back(info.name);
        if (info.name == "engine:test_health") {
            EXPECT_EQ(static_cast<const Health *>(component)->max, 4);
        }
    });
    std::sort(names.begin(), names.end());
    EXPECT_EQ(names, (std::vector<std::string>{"engine:test_health", "engine:test_position"}));

    int none = 0;
    world.ForEachComponentOf(plain, [&](const ComponentInfo &, const void *) { ++none; });
    EXPECT_EQ(none, 0);

    ASSERT_EQ(world.QueryCount(), 2u);
    EXPECT_EQ(world.DescribeQuery(0).name, "engine:test_first");
    EXPECT_EQ(world.DescribeQuery(0).terms, 1u);
    EXPECT_EQ(world.DescribeQuery(1).name, "engine:test_second");
    EXPECT_EQ(world.DescribeQuery(1).terms, 3u);
}

TEST(EntityWorld, TheArchetypeChangesOnlyWhenTheSetOfComponentsChanges) {
    EntityWorld world;
    RegisterAll(world);
    const Entity a = world.Create();
    const Entity b = world.Create();
    world.Set(a, Position{});
    world.Set(b, Position{});
    EXPECT_EQ(world.ArchetypeOf(a), world.ArchetypeOf(b));

    const ArchetypeId before = world.ArchetypeOf(a);
    world.Set(a, Position{5.0, 5.0, 5.0}); // a new value, the same set
    world.Get<Position>(a).x = 9.0;
    EXPECT_EQ(world.ArchetypeOf(a), before);

    world.Set(a, Velocity{});
    EXPECT_NE(world.ArchetypeOf(a), before);
    EXPECT_NE(world.ArchetypeOf(a), world.ArchetypeOf(b));
    world.Remove<Velocity>(a);
    EXPECT_EQ(world.ArchetypeOf(a), before);
}

TEST(EntityWorld, HoldsAHundredThousandEntities) {
    EntityWorld world;
    RegisterAll(world);
    const auto query = world.MakeQuery<Position, const Velocity>("engine:test_many");
    constexpr std::uint32_t kEntities = 100'000;
    std::vector<Entity> entities;
    entities.reserve(kEntities);
    for (std::uint32_t index = 0; index < kEntities; ++index) {
        const Entity entity = world.Create();
        world.Set(entity, Position{static_cast<double>(index), 0.0, 0.0});
        world.Set(entity, Velocity{1.0f, 2.0f, 3.0f});
        entities.push_back(entity);
    }
    EXPECT_EQ(world.EntityCount(), kEntities);

    for (int tick = 0; tick < 3; ++tick) {
        query.EachChunk([](std::span<const Entity>, std::span<Position> positions,
                           std::span<const Velocity> velocities) {
            for (std::size_t row = 0; row < positions.size(); ++row) {
                positions[row].y += static_cast<double>(velocities[row].y);
            }
        });
    }
    EXPECT_DOUBLE_EQ(world.Get<Position>(entities[kEntities / 2]).y, 6.0);

    for (std::uint32_t index = 0; index < kEntities; index += 2) {
        world.Destroy(entities[index]);
    }
    EXPECT_EQ(world.EntityCount(), kEntities / 2);
    EXPECT_EQ(query.Count(), kEntities / 2);
}

#if RUNIC_ASSERTS_ENABLED

TEST(EntityWorldDeathTest, AComponentTypeMustBeRegisteredBeforeItIsUsed) {
    EntityWorld world;
    const Entity entity = world.Create();
    EXPECT_DEATH(world.Set(entity, Position{}), "not registered in this world");
    EXPECT_DEATH(static_cast<void>(world.MakeQuery<const Position>("engine:test_query")),
                 "not registered in this world");
}

TEST(EntityWorldDeathTest, AComponentNameIsNamespaced) {
    EntityWorld world;
    EXPECT_DEATH(world.RegisterComponent<Position>("Position"), "namespace:name");
    EXPECT_DEATH(world.RegisterComponent<Position>("engine:Position"), "namespace:name");
    EXPECT_DEATH(world.RegisterComponent<Position>("engine:"), "namespace:name");
}

TEST(EntityWorldDeathTest, TwoTypesCannotShareAName) {
    EntityWorld world;
    world.RegisterComponent<Position>("engine:test_value");
    EXPECT_DEATH(world.RegisterComponent<Velocity>("engine:test_value"), "under one name");
    EXPECT_DEATH(world.RegisterComponent<Position>("engine:test_other"), "under another name");
}

TEST(EntityWorldDeathTest, ADeadEntityHasNoComponents) {
    EntityWorld world;
    RegisterAll(world);
    const Entity entity = world.Create();
    world.Destroy(entity);
    EXPECT_DEATH(world.Set(entity, Position{}), "not alive");
    EXPECT_DEATH(static_cast<void>(world.Has<Position>(entity)), "not alive");
}

#endif // RUNIC_ASSERTS_ENABLED
