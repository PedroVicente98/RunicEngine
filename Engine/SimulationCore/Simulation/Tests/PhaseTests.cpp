#include "SimulationRuntime.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "EntityStates.hpp"
#include "EntityWorld.hpp"
#include "EventBuffer.hpp"
#include "WorldComponents.hpp"

using namespace Runic;
using namespace Runic::Simulation;

namespace {

constexpr std::array<RealtimePhase, kRealtimePhaseCount> kEveryPhase{
    RealtimePhase::Commands,    RealtimePhase::Movement,    RealtimePhase::PhysicsStructure,
    RealtimePhase::PhysicsStep, RealtimePhase::PostPhysics, RealtimePhase::Queries,
    RealtimePhase::Commit};

struct Hit {
    std::uint32_t value = 0;
};

struct Marked {
    std::uint32_t by = 0;
};

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

} // namespace

TEST(RealtimePhases, RunInTheirFixedOrderWhateverOrderTheirSystemsWereAdded) {
    SimulationRuntime runtime;
    std::vector<std::string> ran;
    // Added last phase first: the phase decides the order, not the time of adding.
    for (auto phase = kEveryPhase.rbegin(); phase != kEveryPhase.rend(); ++phase) {
        AddRealtime(runtime, *phase, [&ran, phase = *phase](const TickContext &) {
            ran.emplace_back(RealtimePhaseName(phase));
        });
    }
    RunRealtimeTick(runtime);

    EXPECT_EQ(ran, (std::vector<std::string>{"commands", "movement", "physics structure",
                                             "physics step", "post physics", "queries", "commit"}));
}

TEST(RealtimePhases, SystemsOfOnePhaseRunInTheOrderTheyWereAdded) {
    SimulationRuntime runtime;
    std::vector<int> ran;
    AddRealtime(runtime, RealtimePhase::Queries, [&ran](const TickContext &) { ran.push_back(1); });
    AddRealtime(runtime, RealtimePhase::Movement,
                [&ran](const TickContext &) { ran.push_back(0); });
    AddRealtime(runtime, RealtimePhase::Queries, [&ran](const TickContext &) { ran.push_back(2); });
    RunRealtimeTick(runtime);
    EXPECT_EQ(ran, (std::vector<int>{0, 1, 2}));
}

TEST(RealtimePhases, TheRealtimeSlotStillRunsBetweenTheProducersAndTheConsumers) {
    SimulationRuntime runtime;
    std::vector<std::string> ran;
    runtime.AddSystem({"engine:test_consumer", ScheduleSlot::Hz20Consumers,
                       [&ran](const TickContext &) { ran.emplace_back("consumer"); }});
    AddRealtime(runtime, RealtimePhase::Commit,
                [&ran](const TickContext &) { ran.emplace_back("commit"); });
    AddRealtime(runtime, RealtimePhase::Commands,
                [&ran](const TickContext &) { ran.emplace_back("commands"); });
    runtime.AddSystem({"engine:test_producer", ScheduleSlot::Hz20Producers,
                       [&ran](const TickContext &) { ran.emplace_back("producer"); }});
    runtime.RunMasterTick(); // tick 0: every domain is due
    EXPECT_EQ(ran, (std::vector<std::string>{"producer", "commands", "commit", "consumer"}));
}

TEST(RealtimePhases, IsRunningNamesThePhaseOfTheSystemThatRuns) {
    SimulationRuntime runtime;
    std::vector<RealtimePhase> seen;
    for (const RealtimePhase phase : kEveryPhase) {
        AddRealtime(runtime, phase, [&runtime, &seen](const TickContext &) {
            for (const RealtimePhase candidate : kEveryPhase) {
                if (runtime.IsRunning(candidate)) {
                    seen.push_back(candidate);
                }
            }
        });
    }
    bool producerSawAPhase = false;
    runtime.AddSystem(
        {"engine:test_producer", ScheduleSlot::Hz20Producers, [&](const TickContext &) {
             for (const RealtimePhase candidate : kEveryPhase) {
                 producerSawAPhase |= runtime.IsRunning(candidate);
             }
         }});
    RunRealtimeTick(runtime);

    EXPECT_EQ(seen, std::vector<RealtimePhase>(kEveryPhase.begin(), kEveryPhase.end()));
    EXPECT_FALSE(producerSawAPhase); // outside the realtime slot no phase runs
    for (const RealtimePhase phase : kEveryPhase) {
        EXPECT_FALSE(runtime.IsRunning(phase)); // nor between steps
    }
}

TEST(Barriers, AFactEmittedInAPhaseIsReadableFromTheNextPhaseOnNotInItsOwn) {
    SimulationRuntime runtime;
    Events::EventBuffer<Hit> hits(runtime.EventHub(), "engine:test_hits");
    Events::EventReader<Hit> samePhase = hits.MakeReader();
    Events::EventReader<Hit> laterPhase = hits.MakeReader();
    std::size_t readInSamePhase = 0;
    std::vector<std::uint32_t> readLater;

    AddRealtime(runtime, RealtimePhase::Movement, [&](const TickContext &) { hits.Emit({7}); });
    AddRealtime(runtime, RealtimePhase::Movement,
                [&](const TickContext &) { readInSamePhase += samePhase.Take().size(); });
    AddRealtime(runtime, RealtimePhase::Queries, [&](const TickContext &) {
        for (const Events::Event<Hit> &event : laterPhase.Take()) {
            readLater.push_back(event.fact.value);
        }
    });
    RunRealtimeTick(runtime);

    EXPECT_EQ(readInSamePhase, 0u);
    EXPECT_EQ(readLater, (std::vector<std::uint32_t>{7}));
    // The fact was flushed once: the reader of the emitting phase gets it after the barrier.
    EXPECT_EQ(samePhase.Take().size(), 1u);
}

TEST(Barriers, EveryOtherSlotIsOnePhaseWithABarrierAfterIt) {
    SimulationRuntime runtime;
    Events::EventBuffer<Hit> hits(runtime.EventHub(), "engine:test_hits");
    Events::EventReader<Hit> reader = hits.MakeReader();
    std::size_t readByCommands = 0;
    runtime.AddSystem({"engine:test_ai", ScheduleSlot::Hz20Producers,
                       [&](const TickContext &) { hits.Emit({1}); }});
    AddRealtime(runtime, RealtimePhase::Commands,
                [&](const TickContext &) { readByCommands += reader.Take().size(); });
    runtime.RunMasterTick(); // tick 0: the 20 Hz producers, then the realtime slot
    EXPECT_EQ(readByCommands, 1u);
}

TEST(Barriers, AFactEmittedBetweenStepsIsReadableInTheNextStepsFirstPhase) {
    SimulationRuntime runtime;
    Events::EventBuffer<Hit> hits(runtime.EventHub(), "engine:test_hits");
    Events::EventReader<Hit> reader = hits.MakeReader();
    std::size_t read = 0;
    AddRealtime(runtime, RealtimePhase::Commands,
                [&](const TickContext &) { read += reader.Take().size(); });
    hits.Emit({3}); // a safe point between steps, such as the application of a service result
    runtime.RunMasterTick();
    EXPECT_EQ(read, 1u);
}

TEST(Barriers, AWorldChangeMadeInAPhaseAppliesAtItsBarrier) {
    SimulationRuntime runtime;
    ECS::EntityWorld &entities = runtime.Entities();
    entities.RegisterComponent<Marked>("engine:test_marked");
    const ECS::Entity entity = entities.Create();
    bool markedInSamePhase = true;
    bool markedInNextPhase = false;

    AddRealtime(runtime, RealtimePhase::Movement, [&](const TickContext &) {
        EXPECT_TRUE(entities.IsDeferred());
        entities.Set(entity, Marked{1}); // adds a component: the entity would move storage
    });
    AddRealtime(runtime, RealtimePhase::Movement,
                [&](const TickContext &) { markedInSamePhase = entities.Has<Marked>(entity); });
    AddRealtime(runtime, RealtimePhase::PostPhysics,
                [&](const TickContext &) { markedInNextPhase = entities.Has<Marked>(entity); });
    RunRealtimeTick(runtime);

    EXPECT_FALSE(markedInSamePhase);
    EXPECT_TRUE(markedInNextPhase);
    EXPECT_FALSE(entities.IsDeferred()); // between steps changes apply at once
}

TEST(Barriers, DestroyingAnEntityInAPhaseTakesEffectAtItsBarrier) {
    SimulationRuntime runtime;
    ECS::EntityWorld &entities = runtime.Entities();
    const ECS::Entity entity = entities.Create();
    bool aliveInSamePhase = false;
    bool aliveInNextPhase = true;
    AddRealtime(runtime, RealtimePhase::Queries,
                [&](const TickContext &) { entities.Destroy(entity); });
    AddRealtime(runtime, RealtimePhase::Queries,
                [&](const TickContext &) { aliveInSamePhase = entities.IsAlive(entity); });
    AddRealtime(runtime, RealtimePhase::Commit,
                [&](const TickContext &) { aliveInNextPhase = entities.IsAlive(entity); });
    RunRealtimeTick(runtime);
    EXPECT_TRUE(aliveInSamePhase);
    EXPECT_FALSE(aliveInNextPhase);
}

TEST(Barriers, ObservableStateFactsArriveAsEventsAfterTheBarrier) {
    SimulationRuntime runtime;
    ECS::EntityStates &states = runtime.States();
    ECS::StateDeclaration declaration;
    declaration.name = "engine:test_stunned";
    declaration.description = "a test flag";
    declaration.category = ECS::StateCategory::HotFlag;
    declaration.owner = "engine:test_system";
    declaration.observable = true;
    const ECS::StateId stunned = states.Declare(declaration);
    const ECS::Entity entity = runtime.Entities().Create();
    states.Attach(entity);

    Events::EventReader<ECS::StateEntered> entered = runtime.StateEnteredFacts().MakeReader();
    Events::EventReader<ECS::StateExited> exited = runtime.StateExitedFacts().MakeReader();
    std::size_t enteredInSamePhase = 0;
    std::vector<ECS::StateEntered> enteredLater;
    std::vector<ECS::StateExited> exitedLater;

    AddRealtime(runtime, RealtimePhase::Movement, [&](const TickContext &) {
        states.Set(entity, stunned);
        enteredInSamePhase += entered.Take().size();
    });
    AddRealtime(runtime, RealtimePhase::Queries, [&](const TickContext &) {
        for (const auto &event : entered.Take()) {
            enteredLater.push_back(event.fact);
        }
        states.Clear(entity, stunned);
    });
    AddRealtime(runtime, RealtimePhase::Commit, [&](const TickContext &) {
        for (const auto &event : exited.Take()) {
            exitedLater.push_back(event.fact);
        }
    });
    RunRealtimeTick(runtime);

    EXPECT_EQ(enteredInSamePhase, 0u);
    ASSERT_EQ(enteredLater.size(), 1u);
    EXPECT_EQ(enteredLater[0].entity, entity);
    EXPECT_EQ(enteredLater[0].state, stunned);
    ASSERT_EQ(exitedLater.size(), 1u);
    EXPECT_EQ(exitedLater[0].entity, entity);
    EXPECT_EQ(runtime.StateEnteredFacts().Name(), kStateEnteredBufferName);
    EXPECT_EQ(runtime.StateExitedFacts().Name(), kStateExitedBufferName);
}

TEST(Barriers, EventIdsComeFromTheSimulationInTheSameOrderOnEveryRun) {
    // Two runs of the same script give their facts the same IDs: the barrier decides the order.
    auto script = [](std::vector<Events::EventId> &ids) {
        SimulationSpec spec;
        spec.id = Core::SimulationId{42};
        SimulationRuntime runtime(spec);
        Events::EventBuffer<Hit> first(runtime.EventHub(), "engine:test_first");
        Events::EventBuffer<Hit> second(runtime.EventHub(), "engine:test_second");
        Events::EventReader<Hit> firstReader = first.MakeReader();
        Events::EventReader<Hit> secondReader = second.MakeReader();
        AddRealtime(runtime, RealtimePhase::Movement, [&](const TickContext &) {
            second.Emit({1});
            first.Emit({2});
            second.Emit({3});
        });
        AddRealtime(runtime, RealtimePhase::Commit, [&](const TickContext &) {
            for (const auto &event : firstReader.Take()) {
                ids.push_back(event.id);
            }
            for (const auto &event : secondReader.Take()) {
                ids.push_back(event.id);
            }
        });
        RunRealtimeTick(runtime);
        RunRealtimeTick(runtime);
    };
    std::vector<Events::EventId> one;
    std::vector<Events::EventId> two;
    script(one);
    script(two);
    ASSERT_EQ(one.size(), 6u);
    EXPECT_EQ(one, two);
    for (const Events::EventId id : one) {
        EXPECT_EQ(id.simulation, Core::SimulationId{42});
    }
}

TEST(SimulationRuntime, OwnsAnEntityWorldWithTheWorldsComponentsRegistered) {
    SimulationRuntime runtime;
    ECS::EntityWorld &entities = runtime.Entities();
    EXPECT_TRUE(entities.IsRegistered<World::WorldTransform>());
    EXPECT_TRUE(entities.IsRegistered<World::Region>());
    const ECS::Entity entity = entities.Create();
    entities.Set(entity, World::WorldTransform{});
    EXPECT_TRUE(entities.Has<World::WorldTransform>(entity));
    EXPECT_EQ(runtime.EventHub().BufferCount(), 2u); // the state facts
}
