#include "DebugStreamRegistry.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include "AllocationCounter.hpp"
#include "Log.hpp"

using namespace Runic::Core;

namespace {

// Drains a stream from a cursor: the texts read, the cursor to continue from and what was missed.
struct Drained {
    std::vector<std::string> texts;
    std::uint64_t cursor = kFirstDebugRecord;
    std::uint64_t missed = 0;
};

void Drain(const DebugStreamRegistry &streams, DebugStreamId id, Drained &into) {
    std::array<std::byte, 256> buffer;
    for (;;) {
        const DebugRecordRead read = streams.Read(id, into.cursor, buffer);
        if (!read.found) {
            return;
        }
        into.texts.emplace_back(reinterpret_cast<const char *>(buffer.data()),
                                std::min<std::size_t>(read.bytes, buffer.size()));
        into.missed += read.missed;
        into.cursor = read.sequence + 1;
    }
}

} // namespace

TEST(DebugStreamRegistry, ADisabledStreamKeepsNothing) {
    DebugStreamRegistry streams;
    const DebugStreamId trace = streams.Add({"test.trace", "A test trace"});
    ASSERT_TRUE(trace.IsValid());
    EXPECT_FALSE(streams.IsEnabled(trace)); // streams start disabled

    streams.Publish(trace, "nobody listens");

    Drained reader;
    Drain(streams, trace, reader);
    EXPECT_TRUE(reader.texts.empty());
    const DebugStreamInfo info = streams.Describe(trace);
    EXPECT_EQ(info.name, "test.trace");
    EXPECT_EQ(info.help, "A test trace");
    EXPECT_FALSE(info.enabled);
    EXPECT_EQ(info.published, 0u);
}

TEST(DebugStreamRegistry, AnEnabledStreamHandsEveryRecordToEachReader) {
    DebugStreamRegistry streams;
    const DebugStreamId trace = streams.Add({"test.trace", ""});
    streams.SetEnabled(trace, true);
    EXPECT_TRUE(streams.IsEnabled(trace));

    streams.Publish(trace, "first");
    streams.Publish(trace, "second");

    Drained editor;
    Drained recorder;
    Drain(streams, trace, editor);
    EXPECT_EQ(editor.texts, (std::vector<std::string>{"first", "second"}));
    EXPECT_EQ(editor.cursor, kFirstDebugRecord + 2);

    streams.Publish(trace, "third");
    Drain(streams, trace, editor);   // continues where it stopped
    Drain(streams, trace, recorder); // a second reader starts from the beginning
    EXPECT_EQ(editor.texts, (std::vector<std::string>{"first", "second", "third"}));
    EXPECT_EQ(recorder.texts, (std::vector<std::string>{"first", "second", "third"}));
    EXPECT_EQ(editor.missed, 0u);
    EXPECT_EQ(streams.Describe(trace).published, 3u);

    streams.SetEnabled(trace, false);
    streams.Publish(trace, "ignored");
    Drain(streams, trace, editor);
    EXPECT_EQ(editor.texts.size(), 3u);
}

TEST(DebugStreamRegistry, RecordsAreBytesOfAnySizeIncludingNone) {
    DebugStreamRegistry streams;
    const DebugStreamId trace = streams.Add({"test.bytes", ""});
    streams.SetEnabled(trace, true);

    struct Sample {
        std::uint64_t tick;
        float value;
    };
    const Sample sample{77, 1.5f};
    streams.Publish(trace, std::as_bytes(std::span<const Sample>(&sample, 1)));
    streams.Publish(trace, std::span<const std::byte>());

    std::array<std::byte, 64> buffer;
    DebugRecordRead read = streams.Read(trace, kFirstDebugRecord, buffer);
    ASSERT_TRUE(read.found);
    ASSERT_EQ(read.bytes, sizeof(Sample));
    Sample copy{};
    std::memcpy(&copy, buffer.data(), sizeof(copy));
    EXPECT_EQ(copy.tick, 77u);
    EXPECT_FLOAT_EQ(copy.value, 1.5f);

    read = streams.Read(trace, read.sequence + 1, buffer);
    ASSERT_TRUE(read.found);
    EXPECT_EQ(read.bytes, 0u);
    EXPECT_FALSE(streams.Read(trace, read.sequence + 1, buffer).found);

    // A buffer smaller than the record gets its first bytes and learns the full size.
    std::array<std::byte, 4> small;
    read = streams.Read(trace, kFirstDebugRecord, small);
    ASSERT_TRUE(read.found);
    EXPECT_EQ(read.bytes, sizeof(Sample));
    EXPECT_EQ(std::memcmp(small.data(), &sample, small.size()), 0);
}

TEST(DebugStreamRegistry, AFullStreamDropsItsOldestRecordsAndTellsTheReader) {
    DebugStreamRegistry streams;
    DebugStreamSpec spec;
    spec.name = "test.small";
    spec.capacityBytes = 64; // room for five records of 8 bytes behind their 4-byte sizes
    const DebugStreamId trace = streams.Add(spec);
    streams.SetEnabled(trace, true);

    for (int record = 0; record < 12; ++record) {
        const std::string text = "record" + std::to_string(10 + record); // 8 bytes each
        streams.Publish(trace, text);
    }

    Drained reader;
    Drain(streams, trace, reader);
    EXPECT_EQ(reader.texts, (std::vector<std::string>{"record17", "record18", "record19",
                                                      "record20", "record21"}));
    EXPECT_EQ(reader.missed, 7u);
    EXPECT_EQ(reader.cursor, kFirstDebugRecord + 12);

    // A record larger than the whole stream is refused and counted.
    streams.Publish(trace, std::string(100, 'x'));
    EXPECT_EQ(streams.Describe(trace).dropped, 1u);
    EXPECT_EQ(streams.Describe(trace).published, 12u);
    Drain(streams, trace, reader);
    EXPECT_EQ(reader.texts.size(), 5u);
}

TEST(DebugStreamRegistry, RecordsWrapAroundTheEndOfTheRing) {
    DebugStreamRegistry streams;
    DebugStreamSpec spec;
    spec.name = "test.wrap";
    spec.capacityBytes = 50;
    const DebugStreamId trace = streams.Add(spec);
    streams.SetEnabled(trace, true);

    Drained reader;
    std::vector<std::string> expected;
    for (int record = 0; record < 200; ++record) {
        const std::string text = "r" + std::to_string(record) + std::string(record % 7, '.');
        streams.Publish(trace, text);
        expected.push_back(text);
        Drain(streams, trace, reader); // read each as it comes: nothing is lost
    }
    EXPECT_EQ(reader.texts, expected);
    EXPECT_EQ(reader.missed, 0u);
}

TEST(DebugStreamRegistry, StreamsAreFoundByNameAndListed) {
    DebugStreamRegistry streams;
    const DebugStreamId first = streams.Add({"test.first", "The first"});
    const DebugStreamId second = streams.Add({"test.second", "The second"});
    EXPECT_EQ(streams.Add({"test.first", "Registered again"}), first);
    EXPECT_EQ(streams.Count(), 2u);
    EXPECT_EQ(streams.Find("test.second"), second);
    EXPECT_FALSE(streams.Find("test.missing").IsValid());
    EXPECT_EQ(streams.IdAt(0), first);
    EXPECT_EQ(streams.IdAt(1), second);
    EXPECT_FALSE(streams.IdAt(2).IsValid());
    EXPECT_EQ(streams.Describe(first).help, "The first");
}

TEST(DebugStreamRegistry, PublishingDoesNotAllocate) {
    DebugStreamRegistry streams;
    const DebugStreamId enabled = streams.Add({"test.enabled", ""});
    const DebugStreamId disabled = streams.Add({"test.disabled", ""});
    streams.SetEnabled(enabled, true);

    std::array<std::byte, 128> buffer;
    std::size_t allocations = 0;
    std::uint64_t read = 0;
    {
        Testing::AllocationScope scope;
        std::uint64_t cursor = kFirstDebugRecord;
        for (int round = 0; round < 20'000; ++round) {
            streams.Publish(enabled, "a trace line of ordinary length");
            streams.Publish(disabled, "never kept");
            const DebugRecordRead record = streams.Read(enabled, cursor, buffer);
            if (record.found) {
                cursor = record.sequence + 1;
                ++read;
            }
        }
        allocations = scope.Count();
    }
    EXPECT_EQ(allocations, 0u);
    EXPECT_EQ(read, 20'000u);
}

TEST(DebugStreamRegistry, PublishersOnSeveralThreadsLoseNoRecord) {
    DebugStreamRegistry streams;
    DebugStreamSpec spec;
    spec.name = "test.threads";
    spec.capacityBytes = 1024 * 1024;
    const DebugStreamId trace = streams.Add(spec);
    streams.SetEnabled(trace, true);

    constexpr std::uint32_t kThreads = 4;
    constexpr std::uint32_t kPerThread = 5'000;
    std::vector<std::thread> threads;
    for (std::uint32_t thread = 0; thread < kThreads; ++thread) {
        threads.emplace_back([&streams, trace, thread] {
            for (std::uint32_t record = 0; record < kPerThread; ++record) {
                const std::uint32_t value = thread * kPerThread + record;
                streams.Publish(trace, std::as_bytes(std::span<const std::uint32_t>(&value, 1)));
            }
        });
    }
    for (std::thread &thread : threads) {
        thread.join();
    }

    std::set<std::uint32_t> seen;
    std::array<std::byte, sizeof(std::uint32_t)> buffer;
    for (std::uint64_t cursor = kFirstDebugRecord;;) {
        const DebugRecordRead read = streams.Read(trace, cursor, buffer);
        if (!read.found) {
            break;
        }
        ASSERT_EQ(read.bytes, sizeof(std::uint32_t));
        ASSERT_EQ(read.missed, 0u);
        std::uint32_t value = 0;
        std::memcpy(&value, buffer.data(), sizeof(value));
        seen.insert(value);
        cursor = read.sequence + 1;
    }
    EXPECT_EQ(seen.size(), kThreads * kPerThread); // every record once, whole
}

TEST(DebugStreamRegistry, AFullRegistryRefusesTheStream) {
    std::uint32_t errors = 0;
    SetLogSink(
        [](const LogRecord &record, void *user) {
            if (record.level == LogLevel::Error) {
                ++*static_cast<std::uint32_t *>(user);
            }
        },
        &errors);
    DebugStreamRegistry streams(1);
    const DebugStreamId first = streams.Add({"test.first", ""});
    const DebugStreamId second = streams.Add({"test.second", ""});
    const DebugStreamId third = streams.Add({"test.third", ""});
    SetLogSink(nullptr, nullptr);

    EXPECT_TRUE(first.IsValid());
    EXPECT_FALSE(second.IsValid());
    EXPECT_FALSE(third.IsValid());
    EXPECT_EQ(errors, 1u);

    streams.SetEnabled(second, true); // ignored
    streams.Publish(second, "ignored");
    EXPECT_FALSE(streams.IsEnabled(second));
    std::array<std::byte, 8> buffer;
    EXPECT_FALSE(streams.Read(second, kFirstDebugRecord, buffer).found);
}
