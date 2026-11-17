#include "CommandQueue.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "EntityWorld.hpp"
#include "MetricsRegistry.hpp"
#include "SimulationRuntime.hpp"

using namespace Runic;
using namespace Runic::Simulation;

namespace {

struct Move {
    ECS::Entity entity;
    float dx = 0.0F;
};

struct Position {
    float x = 0.0F;
};

struct Numbered {
    std::uint32_t source = 0;
    std::uint32_t sequence = 0;
};

void RunRealtimeTick(SimulationRuntime &runtime) {
    const std::uint64_t before = runtime.TicksRun(TickDomain::Hz30);
    while (runtime.TicksRun(TickDomain::Hz30) == before) {
        runtime.RunMasterTick();
    }
}

} // namespace

TEST(CommandQueue, CommandsChangeTheWorldInTheCommandsPhaseInTheOrderTheyWereQueued) {
    SimulationRuntime runtime;
    ECS::EntityWorld &entities = runtime.Entities();
    entities.RegisterComponent<Position>("engine:test_position");
    const ECS::Entity entity = entities.Create();
    entities.Set(entity, Position{});
    CommandQueue<Move> moves(runtime, "engine:test_move");

    std::vector<float> applied;
    runtime.AddSystem({"engine:test_mover", ScheduleSlot::Hz30Realtime,
                       [&](const TickContext &) {
                           moves.Drain([&](const Move &move) {
                               entities.Get<Position>(move.entity).x += move.dx;
                               applied.push_back(move.dx);
                           });
                       },
                       RealtimePhase::Commands});
    float seenByMovement = 0.0F;
    runtime.AddSystem(
        {"engine:test_reader", ScheduleSlot::Hz30Realtime,
         [&](const TickContext &) { seenByMovement = entities.Get<Position>(entity).x; },
         RealtimePhase::Movement});

    moves.Push({entity, 1.0F});
    moves.Push({entity, 2.0F});
    moves.Push({entity, 4.0F});
    EXPECT_EQ(entities.Get<Position>(entity).x, 0.0F); // queued: nothing changed yet
    RunRealtimeTick(runtime);

    EXPECT_EQ(applied, (std::vector<float>{1.0F, 2.0F, 4.0F}));
    EXPECT_EQ(seenByMovement, 7.0F);
}

TEST(CommandQueue, ACommandQueuedAfterARealtimeTickWaitsForTheNextOne) {
    SimulationRuntime runtime;
    CommandQueue<int> queue(runtime, "engine:test_queue");
    std::vector<std::uint64_t> appliedAtTick;
    runtime.AddSystem({"engine:test_owner", ScheduleSlot::Hz30Realtime,
                       [&](const TickContext &context) {
                           queue.Drain(
                               [&](int) { appliedAtTick.push_back(context.masterTick.Value()); });
                       },
                       RealtimePhase::Commands});

    runtime.RunMasterTick(); // tick 0: realtime, nothing queued
    queue.Push(1);
    runtime.RunMasterTick(); // tick 1: no realtime tick, the command waits
    EXPECT_TRUE(appliedAtTick.empty());
    runtime.RunMasterTick(); // tick 2: realtime
    EXPECT_EQ(appliedAtTick, (std::vector<std::uint64_t>{2}));
}

TEST(CommandQueue, ACommandQueuedWhileItsQueueDrainsWaitsForTheNextDrain) {
    SimulationRuntime runtime;
    CommandQueue<int> queue(runtime, "engine:test_queue");
    std::vector<int> applied;
    runtime.AddSystem({"engine:test_owner", ScheduleSlot::Hz30Realtime,
                       [&](const TickContext &) {
                           const std::size_t count = queue.Drain([&](int command) {
                               applied.push_back(command);
                               if (command < 3) {
                                   queue.Push(command + 1); // a command that leads to another
                               }
                           });
                           EXPECT_EQ(count, 1u);
                       },
                       RealtimePhase::Commands});
    queue.Push(1);
    RunRealtimeTick(runtime);
    EXPECT_EQ(applied, (std::vector<int>{1}));
    RunRealtimeTick(runtime);
    RunRealtimeTick(runtime);
    EXPECT_EQ(applied, (std::vector<int>{1, 2, 3}));
}

TEST(CommandQueue, CommandsPushedFromManyThreadsAllApplyEachThreadsInItsOrder) {
    SimulationRuntime runtime;
    CommandQueue<Numbered> queue(runtime, "engine:test_queue");
    constexpr std::uint32_t kThreads = 4;
    constexpr std::uint32_t kPerThread = 5000;
    std::vector<std::uint32_t> lastSeen(kThreads, 0);
    std::uint64_t total = 0;
    bool inOrder = true;
    runtime.AddSystem({"engine:test_owner", ScheduleSlot::Hz30Realtime,
                       [&](const TickContext &) {
                           queue.Drain([&](const Numbered &command) {
                               inOrder &= command.sequence == lastSeen[command.source] + 1;
                               lastSeen[command.source] = command.sequence;
                               ++total;
                           });
                       },
                       RealtimePhase::Commands});

    std::vector<std::thread> producers;
    for (std::uint32_t source = 0; source < kThreads; ++source) {
        producers.emplace_back([&queue, source] {
            for (std::uint32_t sequence = 1; sequence <= kPerThread; ++sequence) {
                queue.Push(Numbered{source, sequence});
            }
        });
    }
    // The simulation keeps ticking while the producers push: each tick takes what has arrived.
    const std::uint64_t expected = std::uint64_t{kThreads} * kPerThread;
    while (total < expected) {
        RunRealtimeTick(runtime);
        std::this_thread::yield();
    }
    for (std::thread &producer : producers) {
        producer.join();
    }
    EXPECT_TRUE(inOrder);
    EXPECT_EQ(total, expected);
    for (std::uint32_t source = 0; source < kThreads; ++source) {
        EXPECT_EQ(lastSeen[source], kPerThread);
    }
}

TEST(CommandQueue, ToolsListEveryQueueWithWhatItHoldsAndHasApplied) {
    SimulationRuntime runtime;
    CommandQueue<int> moves(runtime, "engine:test_move");
    auto casts = std::make_unique<CommandQueue<int>>(runtime, "engine:test_cast");
    runtime.AddSystem({"engine:test_owner", ScheduleSlot::Hz30Realtime,
                       [&](const TickContext &) { moves.Drain([](int) {}); },
                       RealtimePhase::Commands});

    ASSERT_EQ(runtime.CommandQueueCount(), 2u);
    moves.Push(1);
    moves.Push(2);
    casts->Push(3);
    CommandQueueInfo info = runtime.DescribeCommandQueue(0);
    EXPECT_EQ(info.name, "engine:test_move");
    EXPECT_EQ(info.queued, 2u);
    EXPECT_EQ(info.applied, 0u);
    EXPECT_EQ(runtime.DescribeCommandQueue(1).name, "engine:test_cast");
    EXPECT_EQ(runtime.DescribeCommandQueue(1).queued, 1u);

    RunRealtimeTick(runtime);
    info = runtime.DescribeCommandQueue(0);
    EXPECT_EQ(info.queued, 0u);
    EXPECT_EQ(info.applied, 2u);
    EXPECT_EQ(runtime.DescribeCommandQueue(1).queued, 1u); // nobody drains it

    casts.reset(); // a queue that goes leaves the list
    ASSERT_EQ(runtime.CommandQueueCount(), 1u);
    EXPECT_EQ(runtime.DescribeCommandQueue(0).name, "engine:test_move");
}

TEST(CommandQueue, RecordsTheCommandsEachQueueApplied) {
    Core::MetricsRegistry metrics;
    SimulationSpec spec;
    spec.metrics = &metrics;
    SimulationRuntime runtime(spec);
    CommandQueue<int> queue(runtime, "engine:test_move");
    runtime.AddSystem({"engine:test_owner", ScheduleSlot::Hz30Realtime,
                       [&](const TickContext &) { queue.Drain([](int) {}); },
                       RealtimePhase::Commands});
    for (int command = 0; command < 5; ++command) {
        queue.Push(command);
    }
    RunRealtimeTick(runtime);
    queue.Push(5);
    RunRealtimeTick(runtime);

    const Core::CounterId applied = metrics.AddCounter(
        {std::string(kCommandsAppliedMetricPrefix) + "engine:test_move", "hz30", ""});
    EXPECT_EQ(metrics.Read(applied), 6u);
}

#if RUNIC_ASSERTS_ENABLED

TEST(CommandQueueDeathTest, CommandsDoNotApplyOutsideTheCommandsPhase) {
    SimulationRuntime runtime;
    CommandQueue<int> queue(runtime, "engine:test_queue");
    queue.Push(1);
    runtime.AddSystem({"engine:test_late", ScheduleSlot::Hz30Realtime,
                       [&](const TickContext &) { queue.Drain([](int) {}); },
                       RealtimePhase::Movement});
    EXPECT_DEATH(runtime.RunMasterTick(), "commands apply in the Commands phase");
}

TEST(CommandQueueDeathTest, CommandsDoNotApplyBetweenSteps) {
    SimulationRuntime runtime;
    CommandQueue<int> queue(runtime, "engine:test_queue");
    EXPECT_DEATH(queue.Drain([](int) {}), "commands apply in the Commands phase");
}

TEST(CommandQueueDeathTest, CommandsDoNotApplyFromAnotherSlot) {
    SimulationRuntime runtime;
    CommandQueue<int> queue(runtime, "engine:test_queue");
    runtime.AddSystem({"engine:test_ai", ScheduleSlot::Hz20Producers,
                       [&](const TickContext &) { queue.Drain([](int) {}); }});
    EXPECT_DEATH(runtime.RunMasterTick(), "commands apply in the Commands phase");
}

TEST(CommandQueueDeathTest, TheRuntimeOutlivesItsQueues) {
    auto runtime = std::make_unique<SimulationRuntime>();
    EXPECT_DEATH(
        {
            new CommandQueue<int>(*runtime, "engine:test_queue"); // never destroyed
            runtime.reset();
        },
        "destroyed before its command queues");
}

#endif // RUNIC_ASSERTS_ENABLED
