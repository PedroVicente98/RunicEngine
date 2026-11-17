#include "EventBuffer.hpp"
#include "EventId.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

using namespace Runic;
using namespace Runic::Events;

namespace {

// A scripted run of a small simulation: per tick, which facts each of two buffers emits. The
// script stands for a deterministic simulation: the same state gives the same facts.
struct Tick {
    int moves;
    int hits;
};

const std::vector<Tick> kScript = {{2, 0}, {0, 3}, {1, 1}, {0, 0}, {4, 2}, {1, 0}, {3, 3}, {2, 1}};

// Runs ticks [from, to) of the script on a hub and returns the IDs of the events, in read order.
std::vector<EventId> RunScript(EventHub &hub, std::size_t from, std::size_t to) {
    EventBuffer<int> moves(hub, "engine:test_moves");
    EventBuffer<int> hits(hub, "engine:test_hits");
    EventReader<int> moveReader = moves.MakeReader();
    EventReader<int> hitReader = hits.MakeReader();
    std::vector<EventId> ids;
    for (std::size_t tick = from; tick < to; ++tick) {
        for (int index = 0; index < kScript[tick].hits; ++index) {
            hits.Emit(index);
        }
        for (int index = 0; index < kScript[tick].moves; ++index) {
            moves.Emit(index);
        }
        hub.Flush();
        for (const Event<int> &event : moveReader.Take()) {
            ids.push_back(event.id);
        }
        for (const Event<int> &event : hitReader.Take()) {
            ids.push_back(event.id);
        }
    }
    return ids;
}

} // namespace

TEST(EventId, IsASimulationAndASequence) {
    const EventId none;
    EXPECT_FALSE(none.IsValid());
    const EventId first{Core::SimulationId(3), kFirstEventSequence};
    const EventId second{Core::SimulationId(3), 2};
    const EventId other{Core::SimulationId(4), kFirstEventSequence};
    EXPECT_TRUE(first.IsValid());
    EXPECT_FALSE((EventId{Core::SimulationId(3), kNoEventSequence}).IsValid());
    EXPECT_EQ(first, (EventId{Core::SimulationId(3), 1}));
    EXPECT_NE(first, second);
    EXPECT_NE(first, other); // the same sequence in another simulation is another event
    EXPECT_LT(first, second);
    EXPECT_LT(second, other);
    EXPECT_EQ(ToText(second), "3:2");
}

TEST(EventSequence, HandsOutNumbersInARow) {
    EventSequence sequence(Core::SimulationId(9));
    EXPECT_EQ(sequence.Next(), kFirstEventSequence);
    EXPECT_EQ(sequence.Take(3), (EventId{Core::SimulationId(9), 1}));
    EXPECT_EQ(sequence.Take(0), (EventId{Core::SimulationId(9), 4})); // none taken
    EXPECT_EQ(sequence.Take(2), (EventId{Core::SimulationId(9), 4}));
    EXPECT_EQ(sequence.Next(), 6u);

    const EventSequence restored(Core::SimulationId(9), 1'000);
    EXPECT_EQ(restored.Next(), 1'000u);
    EXPECT_EQ(restored.Simulation(), Core::SimulationId(9));
}

TEST(EventId, StaysTheSameWhenTheSimulationIsReplayed) {
    constexpr Core::SimulationId kSimulation{12};

    EventHub original(kSimulation);
    const std::vector<EventId> firstRun = RunScript(original, 0, kScript.size());
    ASSERT_EQ(firstRun.size(), 23u);

    // The whole run again, from nothing: every event has the ID it had the first time.
    EventHub again(kSimulation);
    EXPECT_EQ(RunScript(again, 0, kScript.size()), firstRun);

    // Every ID is used once.
    for (std::size_t index = 0; index < firstRun.size(); ++index) {
        for (std::size_t other = index + 1; other < firstRun.size(); ++other) {
            ASSERT_NE(firstRun[index], firstRun[other]);
        }
    }
}

TEST(EventId, StaysTheSameWhenTheSimulationResumesFromACheckpoint) {
    constexpr Core::SimulationId kSimulation{12};
    constexpr std::size_t kCheckpointTick = 5;

    // One uninterrupted run, and where its sequence was at the checkpoint.
    EventHub uninterrupted(kSimulation);
    const std::vector<EventId> head = RunScript(uninterrupted, 0, kCheckpointTick);
    const std::uint64_t checkpoint = uninterrupted.NextSequence();
    const std::vector<EventId> tail = RunScript(uninterrupted, kCheckpointTick, kScript.size());

    // The process dies after the checkpoint and a new one replays the tail from it.
    EventHub restored(kSimulation, checkpoint);
    EXPECT_EQ(RunScript(restored, kCheckpointTick, kScript.size()), tail);
    EXPECT_EQ(restored.NextSequence(), uninterrupted.NextSequence());
    EXPECT_EQ(head.size() + tail.size(), 23u);

    // Another simulation gives its events other IDs.
    EventHub neighbor(Core::SimulationId(13), checkpoint);
    EXPECT_NE(RunScript(neighbor, kCheckpointTick, kScript.size()), tail);
}
