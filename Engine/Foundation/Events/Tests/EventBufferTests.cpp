#include "EventBuffer.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "Log.hpp"

using namespace Runic;
using namespace Runic::Events;

namespace {

constexpr Core::SimulationId kSimulation{7};

// A fact in the engine's neutral test vocabulary.
struct TestHit {
    std::uint32_t attacker = 0;
    std::uint32_t target = 0;
    std::int32_t amount = 0;

    friend bool operator==(const TestHit &, const TestHit &) = default;
};

template <class T> std::vector<T> Facts(std::span<const Event<T>> events) {
    std::vector<T> facts;
    for (const Event<T> &event : events) {
        facts.push_back(event.fact);
    }
    return facts;
}

template <class T> std::vector<std::uint64_t> Sequences(std::span<const Event<T>> events) {
    std::vector<std::uint64_t> sequences;
    for (const Event<T> &event : events) {
        EXPECT_EQ(event.id.simulation, kSimulation);
        sequences.push_back(event.id.sequence);
    }
    return sequences;
}

} // namespace

TEST(EventBuffer, AnEventIsNotReadableUntilTheHubIsFlushed) {
    EventHub hub(kSimulation);
    EventBuffer<TestHit> hits(hub, "engine:test_hits");
    EventReader<TestHit> reader = hits.MakeReader();
    ASSERT_TRUE(reader.IsValid());

    hits.Emit(TestHit{1, 2, 10});
    EXPECT_TRUE(reader.Take().empty()); // emitted in this phase: not visible yet
    EXPECT_EQ(hits.Describe().pending, 1u);

    EXPECT_EQ(hub.Flush(), 1u); // the barrier
    const std::span<const Event<TestHit>> events = reader.Take();
    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0].fact, (TestHit{1, 2, 10}));
    EXPECT_TRUE(events[0].id.IsValid());
    EXPECT_EQ(hits.Describe().pending, 0u);
}

TEST(EventBuffer, TwoConsumersReadTheSameFact) {
    EventHub hub(kSimulation);
    EventBuffer<TestHit> hits(hub, "engine:test_hits");
    EventReader<TestHit> combatLog = hits.MakeReader();
    EventReader<TestHit> questTracker = hits.MakeReader();

    hits.Emit(TestHit{1, 2, 10});
    hits.Emit(TestHit{2, 1, 4});
    hub.Flush();

    const std::span<const Event<TestHit>> first = combatLog.Take();
    const std::span<const Event<TestHit>> second = questTracker.Take();
    ASSERT_EQ(first.size(), 2u);
    ASSERT_EQ(second.size(), 2u);
    for (std::size_t index = 0; index < first.size(); ++index) {
        EXPECT_EQ(first[index].fact, second[index].fact);
        EXPECT_EQ(first[index].id, second[index].id); // the same fact, the same ID
    }
    EXPECT_EQ(Facts(first), (std::vector<TestHit>{{1, 2, 10}, {2, 1, 4}}));
}

TEST(EventBuffer, AReaderGetsEachEventOnceInOrder) {
    EventHub hub(kSimulation);
    EventBuffer<int> numbers(hub, "engine:test_numbers");
    EventReader<int> reader = numbers.MakeReader();

    std::vector<int> seen;
    int next = 0;
    for (int flush = 0; flush < 50; ++flush) {
        for (int count = 0; count < flush % 4; ++count) {
            numbers.Emit(next++);
        }
        hub.Flush();
        for (const Event<int> &event : reader.Take()) {
            seen.push_back(event.fact);
        }
        EXPECT_TRUE(reader.Take().empty()); // nothing twice
    }
    ASSERT_EQ(seen.size(), static_cast<std::size_t>(next));
    for (int index = 0; index < next; ++index) {
        EXPECT_EQ(seen[static_cast<std::size_t>(index)], index);
    }
    EXPECT_EQ(reader.Missed(), 0u);
}

TEST(EventBuffer, ASlowerConsumerKeepsItsEventsAcrossFlushes) {
    EventHub hub(kSimulation);
    EventBuffer<int> facts(hub, "engine:test_facts");
    EventReader<int> realtime = facts.MakeReader(); // reads after every flush
    EventReader<int> slow = facts.MakeReader(); // reads every third flush, like a 10 Hz consumer

    std::vector<int> slowSeen;
    for (int tick = 0; tick < 30; ++tick) {
        facts.Emit(tick * 2);
        facts.Emit(tick * 2 + 1);
        hub.Flush();
        ASSERT_EQ(realtime.Take().size(), 2u);
        if (tick % 3 == 2) {
            const std::span<const Event<int>> events = slow.Take();
            ASSERT_EQ(events.size(), 6u) << "tick " << tick;
            for (const Event<int> &event : events) {
                slowSeen.push_back(event.fact);
            }
        }
    }
    ASSERT_EQ(slowSeen.size(), 60u);
    for (int index = 0; index < 60; ++index) {
        EXPECT_EQ(slowSeen[static_cast<std::size_t>(index)], index);
    }
}

TEST(EventBuffer, EventsEveryReaderHasReadAreLetGoAtTheNextFlush) {
    EventHub hub(kSimulation);
    EventBuffer<int> facts(hub, "engine:test_facts");
    EventReader<int> fast = facts.MakeReader();
    EventReader<int> slow = facts.MakeReader();

    for (int index = 0; index < 100; ++index) {
        facts.Emit(index);
    }
    hub.Flush();
    EXPECT_EQ(facts.Describe().readable, 100u);
    EXPECT_EQ(fast.Take().size(), 100u);
    hub.Flush();
    EXPECT_EQ(facts.Describe().readable, 100u); // the slow reader has not read them
    EXPECT_EQ(slow.Take().size(), 100u);
    hub.Flush();
    EXPECT_EQ(facts.Describe().readable, 0u);
    EXPECT_EQ(facts.Describe().flushed, 100u);
    EXPECT_EQ(facts.Describe().dropped, 0u);

    // A long run with both readers keeping up never grows.
    for (int tick = 0; tick < 10'000; ++tick) {
        facts.Emit(tick);
        hub.Flush();
        ASSERT_EQ(fast.Take().size(), 1u);
        ASSERT_EQ(slow.Take().size(), 1u);
        ASSERT_LE(facts.Describe().readable, 1u);
    }
}

TEST(EventBuffer, AReaderMadeLaterGetsOnlyLaterEvents) {
    EventHub hub(kSimulation);
    EventBuffer<int> facts(hub, "engine:test_facts");
    EventReader<int> early = facts.MakeReader();
    facts.Emit(1);
    hub.Flush();
    facts.Emit(2); // emitted before the late reader exists, flushed after

    EventReader<int> late = facts.MakeReader();
    hub.Flush();
    facts.Emit(3);
    hub.Flush();

    EXPECT_EQ(Facts(early.Take()), (std::vector<int>{1, 2, 3}));
    EXPECT_EQ(Facts(late.Take()), (std::vector<int>{2, 3}));
}

TEST(EventBuffer, ABufferWithNoReaderKeepsNothing) {
    EventHub hub(kSimulation);
    EventBuffer<int> facts(hub, "engine:test_facts");
    for (int index = 0; index < 1'000; ++index) {
        facts.Emit(index);
    }
    hub.Flush();
    EXPECT_EQ(facts.Describe().readable, 0u);
    EXPECT_EQ(facts.Describe().flushed, 1'000u);
    EXPECT_EQ(facts.Describe().dropped, 0u);                    // nobody was waiting for them
    EXPECT_EQ(hub.NextSequence(), kFirstEventSequence + 1'000); // they still took their IDs
}

TEST(EventBuffer, AReaderThatLeavesNoLongerHoldsEvents) {
    EventHub hub(kSimulation);
    EventBuffer<int> facts(hub, "engine:test_facts");
    EventReader<int> stays = facts.MakeReader();
    {
        EventReader<int> leaves = facts.MakeReader();
        EXPECT_EQ(facts.Describe().readers, 2u);
        facts.Emit(1);
        hub.Flush();
        EXPECT_EQ(stays.Take().size(), 1u);
        hub.Flush();
        EXPECT_EQ(facts.Describe().readable, 1u); // held for the reader that has not read
    }
    EXPECT_EQ(facts.Describe().readers, 1u);
    hub.Flush();
    EXPECT_EQ(facts.Describe().readable, 0u);

    // A moved reader keeps its place; the moved-from one is a reader of nothing.
    facts.Emit(2);
    EventReader<int> moved = std::move(stays);
    EXPECT_FALSE(stays.IsValid()); // NOLINT(bugprone-use-after-move): the state is specified
    hub.Flush();
    EXPECT_EQ(Facts(moved.Take()), (std::vector<int>{2}));
    EXPECT_EQ(facts.Describe().readers, 1u);
}

TEST(EventBuffer, AFullBufferLetsGoOfItsOldestEventsAndTellsTheReader) {
    std::vector<std::string> errors;
    Core::SetLogSink(
        [](const Core::LogRecord &record, void *user) {
            if (record.level == Core::LogLevel::Error) {
                static_cast<std::vector<std::string> *>(user)->emplace_back(record.message);
            }
        },
        &errors);

    EventHub hub(kSimulation);
    EventBuffer<int> facts(hub, "engine:test_small", 8);
    EventReader<int> stuck = facts.MakeReader();
    EventReader<int> keepsUp = facts.MakeReader();

    for (int tick = 0; tick < 5; ++tick) {
        for (int index = 0; index < 4; ++index) {
            facts.Emit(tick * 4 + index);
        }
        hub.Flush();
        EXPECT_EQ(keepsUp.Take().size(), 4u);
        EXPECT_EQ(keepsUp.Missed(), 0u);
    }
    Core::SetLogSink(nullptr, nullptr);

    // 20 events were flushed and the buffer keeps 8: the stuck reader missed the first 12.
    EXPECT_EQ(facts.Describe().readable, 8u);
    EXPECT_EQ(facts.Describe().dropped, 12u);
    EXPECT_EQ(stuck.Missed(), 12u);
    EXPECT_EQ(Facts(stuck.Take()), (std::vector<int>{12, 13, 14, 15, 16, 17, 18, 19}));
    ASSERT_EQ(errors.size(), 1u); // said once, when it began
    EXPECT_NE(errors[0].find("engine:test_small"), std::string::npos);

    // One flush larger than the whole buffer keeps its newest events.
    for (int index = 0; index < 20; ++index) {
        facts.Emit(100 + index);
    }
    hub.Flush();
    EXPECT_EQ(Facts(stuck.Take()), (std::vector<int>{112, 113, 114, 115, 116, 117, 118, 119}));
    EXPECT_EQ(stuck.Missed(), 24u);
}

TEST(EventBuffer, CarriesFactsThatOwnMemory) {
    EventHub hub(kSimulation);
    EventBuffer<std::string> lines(hub, "engine:test_lines");
    EventBuffer<std::unique_ptr<int>> boxes(hub, "engine:test_boxes");
    EventReader<std::string> lineReader = lines.MakeReader();
    EventReader<std::unique_ptr<int>> boxReader = boxes.MakeReader();

    const std::string copied(200, 'a');
    lines.Emit(copied);
    lines.Emit(std::string(300, 'b'));
    boxes.Emit(std::make_unique<int>(42)); // a fact that can only be moved
    hub.Flush();

    const std::span<const Event<std::string>> text = lineReader.Take();
    ASSERT_EQ(text.size(), 2u);
    EXPECT_EQ(text[0].fact, copied);
    EXPECT_EQ(text[1].fact, std::string(300, 'b'));
    const std::span<const Event<std::unique_ptr<int>>> box = boxReader.Take();
    ASSERT_EQ(box.size(), 1u);
    EXPECT_EQ(*box[0].fact, 42);
}

TEST(EventHub, GivesIdsInBufferThenEmissionOrder) {
    EventHub hub(kSimulation);
    EXPECT_EQ(hub.Simulation(), kSimulation);
    EXPECT_EQ(hub.NextSequence(), kFirstEventSequence);
    EventBuffer<TestHit> hits(hub, "engine:test_hits");
    EventBuffer<int> deaths(hub, "engine:test_deaths");
    EventReader<TestHit> hitReader = hits.MakeReader();
    EventReader<int> deathReader = deaths.MakeReader();

    // Emitted interleaved; the IDs follow the buffers' order in the hub, then emission order.
    deaths.Emit(100);
    hits.Emit(TestHit{1, 2, 3});
    deaths.Emit(101);
    hits.Emit(TestHit{4, 5, 6});
    EXPECT_EQ(hub.Flush(), 4u);
    EXPECT_EQ(Sequences(hitReader.Take()), (std::vector<std::uint64_t>{1, 2}));
    EXPECT_EQ(Sequences(deathReader.Take()), (std::vector<std::uint64_t>{3, 4}));

    hits.Emit(TestHit{7, 8, 9});
    hub.Flush();
    EXPECT_EQ(Sequences(hitReader.Take()), (std::vector<std::uint64_t>{5}));
    EXPECT_EQ(hub.NextSequence(), 6u);
    EXPECT_EQ(hub.Flush(), 0u); // nothing emitted: no ID is used
    EXPECT_EQ(hub.NextSequence(), 6u);
}

TEST(EventHub, ListsItsBuffers) {
    EventHub hub(kSimulation);
    EventBuffer<int> first(hub, "engine:test_first");
    {
        EventBuffer<int> second(hub, "engine:test_second");
        const EventReader<int> reader = second.MakeReader();
        second.Emit(1);
        ASSERT_EQ(hub.BufferCount(), 2u);
        EXPECT_EQ(hub.Describe(0).name, "engine:test_first");
        EXPECT_EQ(hub.Describe(1).name, "engine:test_second");
        EXPECT_EQ(hub.Describe(1).pending, 1u);
        EXPECT_EQ(hub.Describe(1).readers, 1u);
        EXPECT_EQ(second.Name(), "engine:test_second");
    }
    EXPECT_EQ(hub.BufferCount(), 1u); // a destroyed buffer leaves the hub
    EXPECT_EQ(hub.Flush(), 0u);
}
