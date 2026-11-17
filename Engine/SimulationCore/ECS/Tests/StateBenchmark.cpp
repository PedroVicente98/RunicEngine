// The state-storage benchmark of ECS-2: what it costs to flip a state under each storage, and
// what a tick costs when 2,000 entities flip states and a system walks them. It runs as a test so
// it cannot rot; the numbers a Release build prints are recorded in the module's ARCHITECTURE.md.

#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "ECSNative.hpp"
#include "EntityStates.hpp"
#include "EntityWorld.hpp"

using namespace Runic::ECS;

namespace {

constexpr std::uint32_t kEntities = 2'000;
constexpr int kStatesPerKind = 8; // states an entity flips in a tick, per storage

struct Body {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    float speed = 1.0f;
};

double Nanoseconds(std::chrono::steady_clock::duration elapsed) {
    return std::chrono::duration<double, std::nano>(elapsed).count();
}

// Runs work(round) `rounds` times and returns the nanoseconds of one run.
template <class Work> double TimeRounds(int rounds, Work &&work) {
    const auto start = std::chrono::steady_clock::now();
    for (int round = 0; round < rounds; ++round) {
        work(round);
    }
    return Nanoseconds(std::chrono::steady_clock::now() - start) / rounds;
}

struct World {
    World() : states(world) {
        world.RegisterComponent<Body>("engine:test_body");
        for (int index = 0; index < kStatesPerKind; ++index) {
            const std::string suffix = std::to_string(index);
            hot.push_back(Declare("engine:test_hot_" + suffix, StateCategory::HotFlag));
            content.push_back(Declare("engine:test_content_" + suffix, StateCategory::ContentFlag));
            value.push_back(Declare("engine:test_value_" + suffix, StateCategory::Value));
            // What the declarations avoid: one plain tag per state, which moves the entity.
            tags.push_back(ecs_new(Native::WorldOf(world)));
        }
        many = Declare("engine:test_many", StateCategory::Relation, TargetCardinality::Many);
        few = Declare("engine:test_few", StateCategory::Relation, TargetCardinality::Few);
        for (std::uint32_t index = 0; index < kEntities; ++index) {
            const Entity entity = world.Create();
            world.Set(entity, Body{});
            states.Attach(entity);
            entities.push_back(entity);
        }
        for (int index = 0; index < 4; ++index) {
            targets.push_back(world.Create());
        }
        walk = world.MakeQuery<Body>("engine:test_walk");
    }

    StateId Declare(const std::string &name, StateCategory category,
                    TargetCardinality cardinality = TargetCardinality::Many) {
        StateDeclaration declaration;
        declaration.name = name;
        declaration.category = category;
        declaration.owner = "engine:test_benchmark";
        declaration.targets = cardinality;
        return states.Declare(declaration);
    }

    // The system every tick ends with: walks every entity's Body.
    void Walk() {
        walk.EachChunk([](std::span<const Entity>, std::span<Body> bodies) {
            for (Body &body : bodies) {
                body.x += static_cast<double>(body.speed);
            }
        });
    }

    EntityWorld world;
    EntityStates states;
    std::vector<StateId> hot;
    std::vector<StateId> content;
    std::vector<StateId> value;
    std::vector<ecs_entity_t> tags;
    StateId many;
    StateId few;
    std::vector<Entity> entities;
    std::vector<Entity> targets;
    EntityQuery<Body> walk;
};

} // namespace

TEST(StateBenchmark, ReportsTheCostOfEachStorageFor2000Entities) {
#if defined(NDEBUG)
    constexpr int kRounds = 40;
#else
    constexpr int kRounds = 4; // a debug build only shows that it runs
#endif
    World w;
    ecs_world_t *native = Native::WorldOf(w.world);
    const double operations = static_cast<double>(kEntities) * 2.0; // a set and a clear each

    // ---- One state flipped on and off on every entity: nanoseconds per operation -------------
    const double toggle = TimeRounds(kRounds,
                                     [&](int) {
                                         for (const Entity entity : w.entities) {
                                             w.states.Set(entity, w.hot[0]);
                                         }
                                         for (const Entity entity : w.entities) {
                                             w.states.Clear(entity, w.hot[0]);
                                         }
                                     }) /
                          operations;
    const double container = TimeRounds(kRounds,
                                        [&](int) {
                                            for (const Entity entity : w.entities) {
                                                w.states.Set(entity, w.content[0]);
                                            }
                                            for (const Entity entity : w.entities) {
                                                w.states.Clear(entity, w.content[0]);
                                            }
                                        }) /
                             operations;
    const double number = TimeRounds(kRounds,
                                     [&](int round) {
                                         for (const Entity entity : w.entities) {
                                             w.states.SetValue(entity, w.value[0], round);
                                         }
                                         for (const Entity entity : w.entities) {
                                             w.states.Clear(entity, w.value[0]);
                                         }
                                     }) /
                          operations;
    const double dontFragment =
        TimeRounds(kRounds,
                   [&](int round) {
                       const Entity target =
                           w.targets[static_cast<std::size_t>(round) % w.targets.size()];
                       for (const Entity entity : w.entities) {
                           w.states.SetTarget(entity, w.many, target);
                       }
                       for (const Entity entity : w.entities) {
                           w.states.Clear(entity, w.many);
                       }
                   }) /
        operations;
    const double fragmenting =
        TimeRounds(kRounds,
                   [&](int round) {
                       const Entity target =
                           w.targets[static_cast<std::size_t>(round) % w.targets.size()];
                       for (const Entity entity : w.entities) {
                           w.states.SetTarget(entity, w.few, target);
                       }
                       for (const Entity entity : w.entities) {
                           w.states.Clear(entity, w.few);
                       }
                   }) /
        operations;
    const double tag = TimeRounds(kRounds,
                                  [&](int) {
                                      for (const Entity entity : w.entities) {
                                          ecs_add_id(native, entity.Raw(), w.tags[0]);
                                      }
                                      for (const Entity entity : w.entities) {
                                          ecs_remove_id(native, entity.Raw(), w.tags[0]);
                                      }
                                  }) /
                       operations;

    // ---- A tick: every entity changes kStatesPerKind states, then a system walks them all ----
    // Entities end each tick in different combinations of states, as in a real fight.
    constexpr double kMillisecond = 1'000'000.0;
    const auto holds = [](std::uint32_t entity, int state, int round) {
        return ((entity >> state) + static_cast<std::uint32_t>(round)) % 2 == 0;
    };
    const double toggleTick =
        TimeRounds(kRounds,
                   [&](int round) {
                       for (std::uint32_t index = 0; index < kEntities; ++index) {
                           for (int state = 0; state < kStatesPerKind; ++state) {
                               if (holds(index, state, round)) {
                                   w.states.Set(w.entities[index],
                                                w.hot[static_cast<std::size_t>(state)]);
                               } else {
                                   w.states.Clear(w.entities[index],
                                                  w.hot[static_cast<std::size_t>(state)]);
                               }
                           }
                       }
                       w.Walk();
                   }) /
        kMillisecond;
    const double containerTick =
        TimeRounds(kRounds,
                   [&](int round) {
                       for (std::uint32_t index = 0; index < kEntities; ++index) {
                           for (int state = 0; state < kStatesPerKind; ++state) {
                               if (holds(index, state, round)) {
                                   w.states.Set(w.entities[index],
                                                w.content[static_cast<std::size_t>(state)]);
                               } else {
                                   w.states.Clear(w.entities[index],
                                                  w.content[static_cast<std::size_t>(state)]);
                               }
                           }
                       }
                       w.Walk();
                   }) /
        kMillisecond;
    const double walkOnly = TimeRounds(kRounds, [&](int) { w.Walk(); }) / kMillisecond;
    std::uint32_t tablesWithTags = 0;
    const double tagTick =
        TimeRounds(kRounds,
                   [&](int round) {
                       for (std::uint32_t index = 0; index < kEntities; ++index) {
                           for (int state = 0; state < kStatesPerKind; ++state) {
                               if (holds(index, state, round)) {
                                   ecs_add_id(native, w.entities[index].Raw(),
                                              w.tags[static_cast<std::size_t>(state)]);
                               } else {
                                   ecs_remove_id(native, w.entities[index].Raw(),
                                                 w.tags[static_cast<std::size_t>(state)]);
                               }
                           }
                       }
                       tablesWithTags = 0;
                       w.walk.EachChunk([&](std::span<const Entity>, std::span<Body> bodies) {
                           ++tablesWithTags;
                           for (Body &body : bodies) {
                               body.x += static_cast<double>(body.speed);
                           }
                       });
                   }) /
        kMillisecond;

    std::printf("\nECS-2 state storage, %u entities (%s build)\n", kEntities,
#if defined(NDEBUG)
                "Release"
#else
                "Debug"
#endif
    );
    std::printf("  one state set or cleared                  ns per operation\n");
    std::printf("    hot flag (toggle)                       %8.1f\n", toggle);
    std::printf("    content flag (container bit)            %8.1f\n", container);
    std::printf("    value (container number)                %8.1f\n", number);
    std::printf("    relation, many targets (DontFragment)   %8.1f\n", dontFragment);
    std::printf("    relation, few targets (moves the entity)%8.1f\n", fragmenting);
    std::printf("    plain tag (moves the entity)            %8.1f\n", tag);
    std::printf("  a tick: %d states changed per entity, then one system walks all  ms per tick\n",
                kStatesPerKind);
    std::printf("    hot flags                               %8.3f\n", toggleTick);
    std::printf("    content flags                           %8.3f\n", containerTick);
    std::printf("    plain tags (%4u storages to walk)      %8.3f\n", tablesWithTags, tagTick);
    std::printf("    the walk alone, entities in one storage %8.3f\n\n", walkOnly);

    // What must hold whatever the machine: the declared storages keep the entities together,
    // and plain tags scatter them.
    std::uint32_t storages = 0;
    for (std::uint32_t index = 0; index < kEntities; ++index) {
        for (int state = 0; state < kStatesPerKind; ++state) {
            ecs_remove_id(native, w.entities[index].Raw(), w.tags[static_cast<std::size_t>(state)]);
        }
    }
    w.walk.EachChunk([&](std::span<const Entity>, std::span<Body>) { ++storages; });
    EXPECT_EQ(storages, 1u);
    EXPECT_GT(tablesWithTags, 100u);
    EXPECT_GT(toggle, 0.0);
}
