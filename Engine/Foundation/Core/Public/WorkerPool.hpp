#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <type_traits>

// The worker pool: the threads every simulation runs its phases on, and the only threads the
// Engine creates for simulation work (RunicDocs › Simulation core clock and threads › Threading
// model). A phase submits jobs under a JobGroup and waits for the group: that wait is the barrier
// nobody passes until every worker has finished. Physics adapts Jolt's job system onto this pool
// (PHYS-4), so no library owns threads.

namespace Runic::Core {

// The slot of the thread a job runs on, from 0 to WorkerCount() - 1. Two jobs that run at the same
// time never share a slot, so a PerWorker<T> indexed by it needs no lock.
using WorkerIndex = std::uint32_t;

// The slot of the thread that drives the pool from outside (the simulation thread): it runs jobs
// too, while it waits for a group.
inline constexpr WorkerIndex kDrivingWorker = 0;

// A pool of one worker creates no thread: every job runs on the driving thread, in the order it
// was submitted. Early slices run this way (RunicDocs: more workers is a configuration change).
inline constexpr std::uint32_t kSingleWorker = 1;
inline constexpr std::uint32_t kMaxWorkers = 64;

// Jobs that may wait in the queue at once. A submit that finds the queue full runs its job on the
// submitting thread instead: nothing blocks, allocates or is dropped.
inline constexpr std::uint32_t kDefaultJobQueueCapacity = 4096;

// The most chunks one ParallelFor splits a range into.
inline constexpr std::uint32_t kMaxParallelChunks = 1u << 20;

// One piece of work: a function and its argument. The function gets the slot it runs on. Whatever
// user points at must stay alive until the job's group has been waited for.
struct Job {
    using Function = void (*)(void *user, WorkerIndex worker);

    Function function = nullptr;
    void *user = nullptr;
};

// Counts the jobs submitted under it that have not finished. It must outlive those jobs, so it is
// waited for before it goes out of scope; it is neither copied nor moved.
class JobGroup {
  public:
    JobGroup() noexcept = default;
    JobGroup(const JobGroup &) = delete;
    JobGroup &operator=(const JobGroup &) = delete;
    ~JobGroup();

    // True when every job submitted under the group so far has finished.
    bool IsDone() const noexcept { return m_Pending.load(std::memory_order_acquire) == kNoJobs; }

  private:
    friend class WorkerPool;

    static constexpr std::uint32_t kNoJobs = 0;

    std::atomic<std::uint32_t> m_Pending{kNoJobs};
};

struct WorkerPoolSpec {
    // Slots, the driving thread included: the pool creates workers - 1 threads.
    std::uint32_t workers = kSingleWorker;
    std::uint32_t queueCapacity = kDefaultJobQueueCapacity;
};

// A run of indices [begin, end) and its position among the chunks of one ParallelFor.
struct ChunkRange {
    std::uint32_t chunk = 0;
    std::uint32_t begin = 0;
    std::uint32_t end = 0;
};

class WorkerPool {
  public:
    // Creates the threads. spec.workers is from kSingleWorker to kMaxWorkers.
    explicit WorkerPool(const WorkerPoolSpec &spec = {});
    // Joins the threads. Every group must have been waited for.
    ~WorkerPool();

    WorkerPool(const WorkerPool &) = delete;
    WorkerPool &operator=(const WorkerPool &) = delete;

    std::uint32_t WorkerCount() const noexcept;

    // Queues a job under a group. Thread-safe: the driving thread and running jobs may submit.
    // It never blocks and never allocates.
    void Submit(const Job &job, JobGroup &group) noexcept;

    // The barrier: returns when every job of the group has finished, running queued jobs (of any
    // group) on the calling thread meanwhile. Called by the driving thread, or by a job that waits
    // for jobs it submitted; one thread at a time drives a pool from outside.
    void Wait(JobGroup &group) noexcept;

    // Splits [0, count) into chunks of chunkSize indices (the last may be shorter), runs
    // body(ChunkRange, WorkerIndex) once per chunk across the workers, and returns when all have
    // run. The chunks depend on count and chunkSize only, never on the worker count, so output
    // kept per chunk merges the same way with one worker and with many. With one worker the chunks
    // run in order on the calling thread.
    template <class Body>
    void ParallelFor(std::uint32_t count, std::uint32_t chunkSize, Body &&body);

    // The slot of the calling thread in this pool: its own for a pool thread, kDrivingWorker for
    // any other thread.
    WorkerIndex CurrentWorker() const noexcept;

    // Jobs a slot has run since the pool was created, for the worker-utilization metric.
    std::uint64_t JobsRunBy(WorkerIndex worker) const noexcept;
    // Jobs that ran on the submitting thread because the queue was full.
    std::uint64_t JobsRunInline() const noexcept;

  private:
    using ChunkFunction = void (*)(void *body, const ChunkRange &range, WorkerIndex worker);

    void RunChunks(std::uint32_t count, std::uint32_t chunkSize, ChunkFunction function,
                   void *body) noexcept;

    struct State;
    std::unique_ptr<State> m_State;
};

template <class Body>
void WorkerPool::ParallelFor(std::uint32_t count, std::uint32_t chunkSize, Body &&body) {
    using Stored = std::remove_reference_t<Body>;
    RunChunks(
        count, chunkSize,
        [](void *stored, const ChunkRange &range, WorkerIndex worker) {
            (*static_cast<Stored *>(stored))(range, worker);
        },
        const_cast<void *>(static_cast<const void *>(std::addressof(body))));
}

} // namespace Runic::Core
