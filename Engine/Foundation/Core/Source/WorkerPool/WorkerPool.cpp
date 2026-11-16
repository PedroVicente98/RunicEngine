// The worker pool (WorkerPool.hpp): a fixed queue under one mutex, the threads that drain it, and
// the barrier a waiting thread helps to reach.

#include "WorkerPool.hpp"

#include <algorithm>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>

#include "Assert.hpp"
#include "PerWorker.hpp"

namespace Runic::Core {

namespace {

constexpr std::uint32_t kNoChunks = 0;
constexpr std::uint64_t kNoJobsRun = 0;

struct QueuedJob {
    Job job;
    JobGroup *group = nullptr;
};

// The pool and slot of the calling thread, set once by each pool thread.
struct ThreadSlot {
    const void *pool = nullptr;
    WorkerIndex worker = kDrivingWorker;
};
thread_local ThreadSlot t_Slot;

// What the runner jobs of one ParallelFor share: they claim chunks from next until none is left.
struct ChunkRun {
    std::atomic<std::uint32_t> next{kNoChunks};
    std::uint32_t chunks = kNoChunks;
    std::uint32_t count = 0;
    std::uint32_t chunkSize = 0;
    void (*function)(void *body, const ChunkRange &range, WorkerIndex worker) = nullptr;
    void *body = nullptr;
};

void RunClaimedChunks(void *user, WorkerIndex worker) {
    auto &run = *static_cast<ChunkRun *>(user);
    for (;;) {
        const std::uint32_t chunk = run.next.fetch_add(1, std::memory_order_relaxed);
        if (chunk >= run.chunks) {
            return;
        }
        ChunkRange range;
        range.chunk = chunk;
        range.begin = chunk * run.chunkSize;
        range.end = static_cast<std::uint32_t>(std::min<std::uint64_t>(
            run.count, static_cast<std::uint64_t>(range.begin) + run.chunkSize));
        run.function(run.body, range, worker);
    }
}

} // namespace

JobGroup::~JobGroup() {
    RUNIC_ASSERT_MSG(IsDone(), "a JobGroup was destroyed before its jobs finished");
}

struct WorkerPool::State {
    explicit State(const WorkerPoolSpec &spec)
        : workers(spec.workers), queue(spec.queueCapacity), jobsRun(spec.workers) {}

    const std::uint32_t workers;

    // The queue: a ring of queue.size() jobs, guarded by mutex. wake is signalled when a job is
    // queued, when a group finishes and when the pool stops.
    std::mutex mutex;
    std::condition_variable wake;
    std::vector<QueuedJob> queue;
    std::uint32_t head = 0;
    std::uint32_t queued = 0;
    bool stopping = false;

    PerWorker<std::atomic<std::uint64_t>> jobsRun;
    std::atomic<std::uint64_t> jobsRunInline{kNoJobsRun};
    std::vector<std::thread> threads;

    bool Pop(QueuedJob &out) noexcept {
        if (queued == 0) {
            return false;
        }
        out = queue[head];
        head = (head + 1) % static_cast<std::uint32_t>(queue.size());
        --queued;
        return true;
    }

    void Run(const QueuedJob &queuedJob, WorkerIndex worker) noexcept {
        queuedJob.job.function(queuedJob.job.user, worker);
        jobsRun[worker].fetch_add(1, std::memory_order_relaxed);
        if (queuedJob.group->m_Pending.fetch_sub(1, std::memory_order_acq_rel) == 1) {
            // The group is done. Taking the mutex before the signal means a thread that has just
            // seen the group unfinished is already waiting when the signal is sent. The group is
            // not touched again: its owner may let it go the moment it reads zero.
            const std::lock_guard lock(mutex);
            wake.notify_all();
        }
    }

    void ThreadMain(WorkerIndex worker) noexcept {
        t_Slot = ThreadSlot{this, worker};
        std::unique_lock lock(mutex);
        for (;;) {
            QueuedJob queuedJob;
            if (Pop(queuedJob)) {
                lock.unlock();
                Run(queuedJob, worker);
                lock.lock();
            } else if (stopping) {
                return;
            } else {
                wake.wait(lock);
            }
        }
    }

    WorkerIndex CurrentWorker() const noexcept {
        return t_Slot.pool == this ? t_Slot.worker : kDrivingWorker;
    }
};

WorkerPool::WorkerPool(const WorkerPoolSpec &spec) {
    RUNIC_ASSERT(spec.workers >= kSingleWorker && spec.workers <= kMaxWorkers);
    RUNIC_ASSERT(spec.queueCapacity > 0);
    m_State = std::make_unique<State>(spec);
    m_State->threads.reserve(spec.workers - kSingleWorker);
    for (WorkerIndex worker = kDrivingWorker + 1; worker < spec.workers; ++worker) {
        m_State->threads.emplace_back(
            [state = m_State.get(), worker] { state->ThreadMain(worker); });
    }
}

WorkerPool::~WorkerPool() {
    {
        const std::lock_guard lock(m_State->mutex);
        RUNIC_ASSERT_MSG(m_State->queued == 0, "a WorkerPool was destroyed with jobs in its queue");
        m_State->stopping = true;
    }
    m_State->wake.notify_all();
    for (std::thread &thread : m_State->threads) {
        thread.join();
    }
}

std::uint32_t WorkerPool::WorkerCount() const noexcept { return m_State->workers; }

void WorkerPool::Submit(const Job &job, JobGroup &group) noexcept {
    RUNIC_ASSERT(job.function != nullptr);
    group.m_Pending.fetch_add(1, std::memory_order_acq_rel);
    const QueuedJob queuedJob{job, &group};
    {
        const std::lock_guard lock(m_State->mutex);
        const auto capacity = static_cast<std::uint32_t>(m_State->queue.size());
        if (m_State->queued < capacity) {
            m_State->queue[(m_State->head + m_State->queued) % capacity] = queuedJob;
            ++m_State->queued;
            m_State->wake.notify_one();
            return;
        }
    }
    m_State->jobsRunInline.fetch_add(1, std::memory_order_relaxed);
    m_State->Run(queuedJob, m_State->CurrentWorker());
}

void WorkerPool::Wait(JobGroup &group) noexcept {
    const WorkerIndex worker = m_State->CurrentWorker();
    std::unique_lock lock(m_State->mutex);
    while (!group.IsDone()) {
        QueuedJob queuedJob;
        if (m_State->Pop(queuedJob)) {
            lock.unlock();
            m_State->Run(queuedJob, worker);
            lock.lock();
        } else {
            m_State->wake.wait(lock);
        }
    }
}

WorkerIndex WorkerPool::CurrentWorker() const noexcept { return m_State->CurrentWorker(); }

std::uint64_t WorkerPool::JobsRunBy(WorkerIndex worker) const noexcept {
    RUNIC_ASSERT(worker < m_State->workers);
    return m_State->jobsRun[worker].load(std::memory_order_relaxed);
}

std::uint64_t WorkerPool::JobsRunInline() const noexcept {
    return m_State->jobsRunInline.load(std::memory_order_relaxed);
}

void WorkerPool::RunChunks(std::uint32_t count, std::uint32_t chunkSize, ChunkFunction function,
                           void *body) noexcept {
    RUNIC_ASSERT(chunkSize > 0);
    if (count == 0) {
        return;
    }
    ChunkRun run;
    run.chunks = (count - 1) / chunkSize + 1;
    RUNIC_ASSERT(run.chunks <= kMaxParallelChunks);
    run.count = count;
    run.chunkSize = chunkSize;
    run.function = function;
    run.body = body;

    // One runner per worker that has a chunk to take; each claims chunks until none is left.
    const std::uint32_t runners = std::min(run.chunks, m_State->workers);
    JobGroup group;
    for (std::uint32_t runner = 0; runner < runners; ++runner) {
        Submit(Job{&RunClaimedChunks, &run}, group);
    }
    Wait(group);
}

} // namespace Runic::Core
