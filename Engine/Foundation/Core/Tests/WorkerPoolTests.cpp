#include "PerWorker.hpp"
#include "WorkerPool.hpp"

#include <gtest/gtest.h>

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <set>
#include <thread>
#include <vector>

#include "AllocationCounter.hpp"

using namespace Runic::Core;

namespace {

struct CountingJob {
    std::atomic<std::uint32_t> runs{0};
    std::atomic<std::uint32_t> lastWorker{0};

    static void Run(void *user, WorkerIndex worker) {
        auto &job = *static_cast<CountingJob *>(user);
        job.runs.fetch_add(1);
        job.lastWorker.store(worker);
    }
};

WorkerPoolSpec Workers(std::uint32_t workers) {
    WorkerPoolSpec spec;
    spec.workers = workers;
    return spec;
}

} // namespace

TEST(WorkerPool, RunsEveryJobExactlyOnce) {
    WorkerPool pool(Workers(4));
    EXPECT_EQ(pool.WorkerCount(), 4u);

    std::vector<CountingJob> jobs(2'000);
    JobGroup group;
    for (CountingJob &job : jobs) {
        pool.Submit(Job{&CountingJob::Run, &job}, group);
    }
    pool.Wait(group);

    EXPECT_TRUE(group.IsDone());
    std::uint64_t total = 0;
    for (const CountingJob &job : jobs) {
        ASSERT_EQ(job.runs.load(), 1u);
        ASSERT_LT(job.lastWorker.load(), pool.WorkerCount());
        ++total;
    }
    std::uint64_t counted = pool.JobsRunInline();
    EXPECT_EQ(counted, 0u); // the queue held every job
    for (WorkerIndex worker = 0; worker < pool.WorkerCount(); ++worker) {
        counted += pool.JobsRunBy(worker);
    }
    EXPECT_EQ(counted, total);
}

TEST(WorkerPool, OneWorkerRunsJobsInSubmissionOrderOnTheCallingThread) {
    WorkerPool pool; // the default: one worker, no thread
    EXPECT_EQ(pool.WorkerCount(), kSingleWorker);

    struct Recorder {
        std::vector<int> order;
        std::set<std::thread::id> threads;
    } recorder;
    struct Item {
        Recorder *recorder;
        int value;
    };
    std::array<Item, 5> items{
        {{&recorder, 0}, {&recorder, 1}, {&recorder, 2}, {&recorder, 3}, {&recorder, 4}}};

    JobGroup group;
    for (Item &item : items) {
        pool.Submit(Job{[](void *user, WorkerIndex worker) {
                            auto &entry = *static_cast<Item *>(user);
                            EXPECT_EQ(worker, kDrivingWorker);
                            entry.recorder->order.push_back(entry.value);
                            entry.recorder->threads.insert(std::this_thread::get_id());
                        },
                        &item},
                    group);
    }
    EXPECT_TRUE(recorder.order.empty()); // queued until the barrier
    pool.Wait(group);

    EXPECT_EQ(recorder.order, (std::vector<int>{0, 1, 2, 3, 4}));
    EXPECT_EQ(recorder.threads, (std::set<std::thread::id>{std::this_thread::get_id()}));
    EXPECT_EQ(pool.JobsRunBy(kDrivingWorker), 5u);
}

TEST(WorkerPool, JobsRunOnPoolThreadsAtTheSameTime) {
    constexpr std::uint32_t kWorkers = 3;
    WorkerPool pool(Workers(kWorkers));

    // Each job waits until every worker is inside a job: that only ends if the jobs run on
    // kWorkers different threads at once.
    struct Gate {
        std::atomic<std::uint32_t> inside{0};
        std::mutex mutex;
        std::set<std::thread::id> threads;
        std::set<WorkerIndex> workers;
    } gate;

    JobGroup group;
    for (std::uint32_t job = 0; job < kWorkers; ++job) {
        pool.Submit(Job{[](void *user, WorkerIndex worker) {
                            auto &shared = *static_cast<Gate *>(user);
                            shared.inside.fetch_add(1);
                            while (shared.inside.load() < kWorkers) {
                                std::this_thread::yield();
                            }
                            const std::lock_guard lock(shared.mutex);
                            shared.threads.insert(std::this_thread::get_id());
                            shared.workers.insert(worker);
                        },
                        &gate},
                    group);
    }
    pool.Wait(group);

    EXPECT_EQ(gate.threads.size(), kWorkers);
    EXPECT_EQ(gate.workers, (std::set<WorkerIndex>{0, 1, 2}));      // no two shared a slot
    EXPECT_TRUE(gate.threads.contains(std::this_thread::get_id())); // the driving thread helped
}

TEST(WorkerPool, JobsThatRunTogetherNeverShareASlot) {
    constexpr std::uint32_t kWorkers = 4;
    WorkerPool pool(Workers(kWorkers));

    struct Shared {
        PerWorker<std::atomic<std::uint32_t>> busy{kWorkers};
        std::atomic<std::uint32_t> clashes{0};
    } shared;

    JobGroup group;
    for (int job = 0; job < 4'000; ++job) {
        pool.Submit(Job{[](void *user, WorkerIndex worker) {
                            auto &state = *static_cast<Shared *>(user);
                            if (state.busy[worker].exchange(1) != 0) {
                                state.clashes.fetch_add(1);
                            }
                            std::this_thread::yield();
                            state.busy[worker].store(0);
                        },
                        &shared},
                    group);
    }
    pool.Wait(group);

    EXPECT_EQ(shared.clashes.load(), 0u);
}

TEST(WorkerPool, WaitReturnsOnlyAfterTheWholeGroupRan) {
    WorkerPool pool(Workers(4));

    for (int round = 0; round < 200; ++round) {
        std::atomic<std::uint32_t> finished{0};
        JobGroup group;
        constexpr std::uint32_t kJobs = 32;
        for (std::uint32_t job = 0; job < kJobs; ++job) {
            pool.Submit(Job{[](void *user, WorkerIndex) {
                                std::this_thread::yield();
                                static_cast<std::atomic<std::uint32_t> *>(user)->fetch_add(1);
                            },
                            &finished},
                        group);
        }
        pool.Wait(group);
        ASSERT_EQ(finished.load(), kJobs) << "round " << round;
        ASSERT_TRUE(group.IsDone());
    }
}

TEST(WorkerPool, AJobMaySubmitMoreJobsToItsGroup) {
    WorkerPool pool(Workers(3));

    struct Tree {
        WorkerPool *pool;
        JobGroup *group;
        std::atomic<std::uint32_t> leaves{0};

        static void Leaf(void *user, WorkerIndex) {
            static_cast<Tree *>(user)->leaves.fetch_add(1);
        }
        static void Branch(void *user, WorkerIndex) {
            auto &tree = *static_cast<Tree *>(user);
            for (int leaf = 0; leaf < 10; ++leaf) {
                tree.pool->Submit(Job{&Tree::Leaf, &tree}, *tree.group);
            }
        }
    };

    JobGroup group;
    Tree tree{&pool, &group};
    for (int branch = 0; branch < 20; ++branch) {
        pool.Submit(Job{&Tree::Branch, &tree}, group);
    }
    pool.Wait(group);

    EXPECT_EQ(tree.leaves.load(), 200u);
}

TEST(WorkerPool, AFullQueueRunsTheJobOnTheSubmittingThread) {
    WorkerPoolSpec spec;
    spec.workers = kSingleWorker;
    spec.queueCapacity = 4;
    WorkerPool pool(spec);

    std::vector<CountingJob> jobs(10);
    JobGroup group;
    std::uint32_t ranBeforeTheBarrier = 0;
    for (CountingJob &job : jobs) {
        pool.Submit(Job{&CountingJob::Run, &job}, group);
        ranBeforeTheBarrier += job.runs.load();
    }
    EXPECT_EQ(ranBeforeTheBarrier, 6u); // four waited in the queue; six ran inside Submit
    EXPECT_EQ(pool.JobsRunInline(), 6u);
    pool.Wait(group);

    for (const CountingJob &job : jobs) {
        EXPECT_EQ(job.runs.load(), 1u);
    }
}

TEST(WorkerPool, ParallelForCoversEveryIndexOnce) {
    for (const std::uint32_t workers : {1u, 2u, 5u}) {
        WorkerPool pool(Workers(workers));
        constexpr std::uint32_t kCount = 10'007;
        std::vector<std::atomic<std::uint8_t>> visits(kCount);

        pool.ParallelFor(kCount, 64, [&](const ChunkRange &range, WorkerIndex worker) {
            EXPECT_LT(worker, pool.WorkerCount());
            for (std::uint32_t index = range.begin; index < range.end; ++index) {
                visits[index].fetch_add(1);
            }
        });

        for (std::uint32_t index = 0; index < kCount; ++index) {
            ASSERT_EQ(visits[index].load(), 1)
                << "index " << index << ", " << workers << " workers";
        }
    }
}

TEST(WorkerPool, ParallelForChunksDoNotDependOnTheWorkerCount) {
    constexpr std::uint32_t kCount = 1'000;
    constexpr std::uint32_t kChunk = 96;
    constexpr std::uint32_t kChunks = 11; // ten of 96 and one of 40

    auto chunksWith = [&](std::uint32_t workers) {
        WorkerPool pool(Workers(workers));
        std::vector<ChunkRange> chunks(kChunks);
        std::atomic<std::uint32_t> calls{0};
        pool.ParallelFor(kCount, kChunk, [&](const ChunkRange &range, WorkerIndex) {
            chunks.at(range.chunk) = range; // each chunk writes its own slot
            calls.fetch_add(1);
        });
        EXPECT_EQ(calls.load(), kChunks);
        return chunks;
    };

    const std::vector<ChunkRange> one = chunksWith(1);
    const std::vector<ChunkRange> many = chunksWith(4);
    ASSERT_EQ(one.size(), many.size());
    for (std::uint32_t chunk = 0; chunk < kChunks; ++chunk) {
        EXPECT_EQ(one[chunk].chunk, chunk);
        EXPECT_EQ(one[chunk].begin, chunk * kChunk);
        EXPECT_EQ(one[chunk].end, chunk + 1 == kChunks ? kCount : (chunk + 1) * kChunk);
        EXPECT_EQ(many[chunk].begin, one[chunk].begin);
        EXPECT_EQ(many[chunk].end, one[chunk].end);
    }
}

TEST(WorkerPool, ParallelForWithOneWorkerRunsTheChunksInOrder) {
    WorkerPool pool;
    std::vector<std::uint32_t> order;
    pool.ParallelFor(10, 3, [&](const ChunkRange &range, WorkerIndex worker) {
        EXPECT_EQ(worker, kDrivingWorker);
        order.push_back(range.chunk);
    });
    EXPECT_EQ(order, (std::vector<std::uint32_t>{0, 1, 2, 3}));

    bool ran = false;
    pool.ParallelFor(0, 3, [&](const ChunkRange &, WorkerIndex) { ran = true; });
    EXPECT_FALSE(ran); // an empty range runs nothing
}

TEST(WorkerPool, SubmitAndWaitDoNotAllocate) {
    for (const std::uint32_t workers : {1u, 3u}) {
        WorkerPool pool(Workers(workers));
        std::array<CountingJob, 64> jobs;
        std::atomic<std::uint64_t> sum{0};

        std::size_t allocations = 0;
        {
            Testing::AllocationScope scope;
            for (int round = 0; round < 50; ++round) {
                JobGroup group;
                for (CountingJob &job : jobs) {
                    pool.Submit(Job{&CountingJob::Run, &job}, group);
                }
                pool.Wait(group);
                pool.ParallelFor(1'000, 100, [&](const ChunkRange &range, WorkerIndex) {
                    sum.fetch_add(range.end - range.begin);
                });
            }
            allocations = scope.Count();
        }

        EXPECT_EQ(allocations, 0u) << workers << " workers";
        EXPECT_EQ(sum.load(), 50'000u);
        EXPECT_EQ(jobs[0].runs.load(), 50u);
    }
}

TEST(WorkerPool, AJobMayWaitForJobsItSubmitted) {
    WorkerPool pool(Workers(3));

    struct Outer {
        WorkerPool *pool;
        std::atomic<std::uint32_t> inner{0};
        std::atomic<std::uint32_t> seenByOuter{0};

        static void Inner(void *user, WorkerIndex) {
            static_cast<Outer *>(user)->inner.fetch_add(1);
        }
        static void Run(void *user, WorkerIndex worker) {
            auto &outer = *static_cast<Outer *>(user);
            EXPECT_EQ(outer.pool->CurrentWorker(), worker);
            JobGroup nested;
            for (int job = 0; job < 8; ++job) {
                outer.pool->Submit(Job{&Outer::Inner, &outer}, nested);
            }
            outer.pool->Wait(nested);
            outer.seenByOuter.fetch_add(1);
        }
    };

    Outer outer{&pool};
    JobGroup group;
    for (int job = 0; job < 6; ++job) {
        pool.Submit(Job{&Outer::Run, &outer}, group);
    }
    pool.Wait(group);

    EXPECT_EQ(outer.inner.load(), 48u);
    EXPECT_EQ(outer.seenByOuter.load(), 6u);
}

TEST(PerWorker, GivesEachWorkerItsOwnCacheLines) {
    PerWorker<std::uint32_t> counters(8);
    EXPECT_EQ(counters.Workers(), 8u);
    for (WorkerIndex worker = 0; worker < counters.Workers(); ++worker) {
        EXPECT_EQ(counters[worker], 0u); // value-initialized
        const auto address = reinterpret_cast<std::uintptr_t>(&counters[worker]);
        EXPECT_EQ(address % kCacheLineBytes, 0u);
        if (worker > 0) {
            const auto previous = reinterpret_cast<std::uintptr_t>(&counters[worker - 1]);
            EXPECT_GE(address - previous, kCacheLineBytes);
        }
    }
}

TEST(PerWorker, WorkersWriteSideBySideAndTheOwnerMergesInOrder) {
    constexpr std::uint32_t kWorkers = 4;
    WorkerPool pool(Workers(kWorkers));
    PerWorker<std::uint64_t> sums(pool.WorkerCount());

    constexpr std::uint32_t kCount = 100'000;
    pool.ParallelFor(kCount, 1'000, [&](const ChunkRange &range, WorkerIndex worker) {
        for (std::uint32_t index = range.begin; index < range.end; ++index) {
            sums[worker] += index; // no lock: the slot is this worker's alone
        }
    });

    std::uint64_t total = 0;
    for (WorkerIndex worker = 0; worker < sums.Workers(); ++worker) {
        total += sums[worker];
    }
    EXPECT_EQ(total, static_cast<std::uint64_t>(kCount) * (kCount - 1) / 2);
}
