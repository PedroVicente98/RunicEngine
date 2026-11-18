#pragma once

#include <Jolt/Jolt.h>

#include <Jolt/Core/FixedSizeFreeList.h>
#include <Jolt/Core/JobSystemWithBarrier.h>

#include <cstdint>

#include "WorkerPool.hpp"

// Jolt's job system on the Core worker pool (PHYS-4; RunicDocs › Threading model: "Jolt runs on the
// same worker pool"). Jolt asks a job system for jobs and barriers; this one keeps Jolt's jobs in a
// fixed free list and hands each job that becomes ready to the pool as a Core job, so Jolt never
// owns a thread. Private to Runic::Physics.
//
// Jolt's barriers (JobSystemWithBarrier) do the waiting: the stepping thread, inside
// PhysicsSystem::Update, runs the jobs of the barrier it waits for while the pool's workers run the
// same jobs from the pool's queue; a job runs once, whoever takes it first. A world without a pool,
// or with a pool of one worker, queues nothing: the barrier runs every job on the stepping thread,
// as Jolt's own thread pool does with no threads.

namespace Runic::Physics::Detail {

class PoolJobSystem final : public JPH::JobSystemWithBarrier {
  public:
    // pool may be null. maxJobs and maxBarriers are the most a step has at once.
    PoolJobSystem(Core::WorkerPool *pool, std::uint32_t maxJobs, std::uint32_t maxBarriers);
    ~PoolJobSystem() override;

    PoolJobSystem(const PoolJobSystem &) = delete;
    PoolJobSystem &operator=(const PoolJobSystem &) = delete;

    int GetMaxConcurrency() const override;
    JobHandle CreateJob(const char *name, JPH::ColorArg color, const JobFunction &function,
                        JPH::uint32 dependencies) override;

    // After PhysicsSystem::Update: the pool's queue may still hold jobs Jolt's barriers already ran
    // (each keeps a reference to its job); this waits for them, so a step leaves nothing behind.
    void Drain();

  protected:
    void QueueJob(Job *job) override;
    void QueueJobs(Job **jobs, JPH::uint count) override;
    void FreeJob(Job *job) override;

  private:
    using AvailableJobs = JPH::FixedSizeFreeList<Job>;

    // The Core job: runs Jolt's job unless a barrier ran it first, then drops the queue's reference.
    static void Run(void *job, Core::WorkerIndex worker);

    Core::WorkerPool *m_Pool = nullptr; // null when nothing is queued (no pool, or one worker)
    int m_Concurrency = 0;
    AvailableJobs m_Jobs;
    Core::JobGroup m_Group;
};

} // namespace Runic::Physics::Detail
