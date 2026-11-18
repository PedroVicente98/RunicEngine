// Jolt's job system on the Core worker pool (PoolJobSystem.hpp).

#include "JobSystem/PoolJobSystem.hpp"

#include <thread>

#include "Assert.hpp"

namespace Runic::Physics::Detail {

namespace {

// The concurrency Jolt plans its step for when every job runs on the stepping thread.
constexpr int kSteppingThreadOnly = 1;

} // namespace

PoolJobSystem::PoolJobSystem(Core::WorkerPool *pool, std::uint32_t maxJobs,
                             std::uint32_t maxBarriers)
    : JobSystemWithBarrier(maxBarriers) {
    const bool queues = pool != nullptr && pool->WorkerCount() > Core::kSingleWorker;
    m_Pool = queues ? pool : nullptr;
    m_Concurrency = queues ? static_cast<int>(pool->WorkerCount()) : kSteppingThreadOnly;
    // Every job is constructed in place in one block of pages, allocated once.
    m_Jobs.Init(maxJobs, maxJobs);
}

PoolJobSystem::~PoolJobSystem() { Drain(); }

int PoolJobSystem::GetMaxConcurrency() const { return m_Concurrency; }

JPH::JobSystem::JobHandle PoolJobSystem::CreateJob(const char *name, JPH::ColorArg color,
                                                   const JobFunction &function,
                                                   JPH::uint32 dependencies) {
    JPH::uint32 index = m_Jobs.ConstructObject(name, color, this, function, dependencies);
    // A step never needs more than maxJobs at once; should one, jobs the pool still holds free
    // their places as the workers drop them.
    while (index == AvailableJobs::cInvalidObjectIndex) {
        RUNIC_ASSERT_MSG(m_Pool != nullptr,
                         "Physics: a step needs more jobs than its job system holds");
        std::this_thread::yield();
        index = m_Jobs.ConstructObject(name, color, this, function, dependencies);
    }
    Job *job = &m_Jobs.Get(index);
    // The handle keeps a reference: the job may be queued below and finish at once.
    JobHandle handle(job);
    if (dependencies == 0) {
        QueueJob(job);
    }
    return handle;
}

void PoolJobSystem::QueueJob(Job *job) {
    if (m_Pool == nullptr) {
        return; // the barrier runs it on the stepping thread
    }
    job->AddRef(); // the queue's reference, dropped by Run
    m_Pool->Submit(Core::Job{&PoolJobSystem::Run, job}, m_Group);
}

void PoolJobSystem::QueueJobs(Job **jobs, JPH::uint count) {
    for (JPH::uint i = 0; i < count; ++i) {
        QueueJob(jobs[i]);
    }
}

void PoolJobSystem::FreeJob(Job *job) { m_Jobs.DestructObject(job); }

void PoolJobSystem::Drain() {
    if (m_Pool != nullptr) {
        m_Pool->Wait(m_Group);
    }
}

void PoolJobSystem::Run(void *user, Core::WorkerIndex) {
    Job *job = static_cast<Job *>(user);
    job->Execute(); // nothing when a barrier already ran it
    job->Release();
}

} // namespace Runic::Physics::Detail
