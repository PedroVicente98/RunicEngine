// GDT-2: the world and ECS inspectors' model over a local simulation, without Godot.

#include "WorldInspector.hpp"

#include <gtest/gtest.h>

#include <string>

#include "CommandQueue.hpp"
#include "EntityWorld.hpp"
#include "SimulationRuntime.hpp"
#include "WorldComponents.hpp"

using namespace Runic;
using namespace Runic::GodotTools;

namespace {

struct Health {
    int points = 0;
};

void FormatHealth(const void *component, std::string &out) {
    out = std::to_string(static_cast<const Health *>(component)->points) + " hp";
}

} // namespace

TEST(WorldInspector, SummarizesTheLocalSimulation) {
    Simulation::SimulationSpec spec;
    spec.id = Core::SimulationId{7};
    Simulation::SimulationRuntime runtime(spec);
    ECS::EntityWorld &world = runtime.Entities();
    world.RegisterComponent<Health>("engine:test_health");
    for (int i = 0; i < 3; ++i) {
        world.Set(world.Create(), Health{i});
    }
    const ECS::Entity region = world.Create();
    world.Set(region, World::Region{});
    world.MakeQuery<const Health>("engine:test_health_scan");
    Simulation::CommandQueue<int> moves(runtime, "engine:test_moves");
    runtime.RunMasterTick();

    const WorldInspector inspector(runtime);
    const WorldSummary summary = inspector.Summary();
    EXPECT_EQ(summary.simulation, Core::SimulationId{7});
    EXPECT_EQ(summary.masterTick, 1u);
    EXPECT_EQ(summary.entities, 4u);
    EXPECT_GE(summary.componentTypes, 3u); // World's two and the test's
    EXPECT_EQ(summary.queries, 1u);
    EXPECT_EQ(summary.commandQueues, 1u);
    EXPECT_EQ(summary.regions, 1u);
    EXPECT_EQ(inspector.Queries(), (std::vector<std::string>{"engine:test_health_scan (1 terms)"}));
    moves.Push(1);
    EXPECT_EQ(inspector.CommandQueues(),
              (std::vector<std::string>{"engine:test_moves: 1 queued, 0 applied"}));
}

TEST(WorldInspector, ShowsAnEntitysComponentsWithTheirValues) {
    Simulation::SimulationRuntime runtime;
    ECS::EntityWorld &world = runtime.Entities();
    ECS::ComponentOptions options;
    options.format = &FormatHealth;
    world.RegisterComponent<Health>("engine:test_health", options);
    const ECS::Entity hero = world.Create();
    world.Set(hero, Health{42});
    world.Set(hero, World::WorldTransform{{1'000'000.5, 2.0, -3.25}, World::kNoRotation});

    const WorldInspector inspector(runtime);
    const InspectedEntity inspected = inspector.Inspect(hero);
    ASSERT_EQ(inspected.components.size(), 2u);
    bool sawHealth = false;
    bool sawTransform = false;
    for (const InspectedComponent &component : inspected.components) {
        if (component.name == "engine:test_health") {
            sawHealth = true;
            EXPECT_EQ(component.value, "42 hp");
        } else if (component.name == World::kWorldTransformName) {
            sawTransform = true;
            EXPECT_NE(component.value.find("1000000.5"), std::string::npos) << component.value;
        }
    }
    EXPECT_TRUE(sawHealth);
    EXPECT_TRUE(sawTransform);
    EXPECT_FALSE(inspected.label.empty());

    world.Destroy(hero);
    EXPECT_TRUE(inspector.Inspect(hero).components.empty());
}

TEST(WorldInspector, ListsAPageOfEntitiesInAStableOrder) {
    Simulation::SimulationRuntime runtime;
    ECS::EntityWorld &world = runtime.Entities();
    for (int i = 0; i < 20; ++i) {
        world.Create();
    }
    const WorldInspector inspector(runtime);
    const auto all = inspector.Entities();
    EXPECT_EQ(all.size(), 20u);
    EXPECT_TRUE(std::is_sorted(all.begin(), all.end(),
                               [](ECS::Entity a, ECS::Entity b) { return a.Raw() < b.Raw(); }));
    EXPECT_EQ(inspector.Entities(5).size(), 5u);
    EXPECT_EQ(inspector.Entities(), all);
}
