#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "MetricsRegistry.hpp"
#include "SimulationRuntime.hpp"

// Commands: the only door player and AI input has into a simulation's world. A command is a plain
// request addressed to the system that owns what would change ("move this entity that way"). It
// is queued from anywhere, at any time: a network thread, a 20 Hz AI producer, a tool. It changes
// nothing until the owner applies it, in the Commands phase of the next realtime tick, in the
// order it was queued (RunicDocs › Realtime 30 Hz phase, step 1; CLAUDE.md: I/O threads only
// enqueue).

namespace Runic::Simulation {

namespace Detail {

// What a command queue is whatever its command type: its name, its counts, its place among the
// runtime's queues.
class CommandQueueBase {
  public:
    CommandQueueBase(const CommandQueueBase &) = delete;
    CommandQueueBase &operator=(const CommandQueueBase &) = delete;

    std::string_view Name() const noexcept { return m_Name; }
    // The queue's counts. From any thread: each count is exact on its own.
    CommandQueueInfo Describe() const noexcept;

  protected:
    CommandQueueBase(SimulationRuntime &runtime, std::string_view name);
    ~CommandQueueBase();

    // Asserts that a system of the Commands phase is running: the only time commands apply.
    void CheckCommandsPhase() const noexcept;
    void CountQueued(std::size_t queued) noexcept {
        m_Queued.store(queued, std::memory_order_relaxed);
    }
    void CountApplied(std::size_t applied) noexcept;

  private:
    SimulationRuntime &m_Runtime;
    std::string m_Name;
    // Written by whoever pushes (queued) and by the driving thread (applied); read by tools.
    std::atomic<std::size_t> m_Queued{0};
    std::atomic<std::uint64_t> m_Applied{0};
    Core::CounterId m_AppliedMetric;
};

} // namespace Detail

template <class T> class CommandQueue final : public Detail::CommandQueueBase {
  public:
    // Joins the runtime's queues. The runtime outlives the queue. The name is for tools and
    // metrics ("engine:test_move").
    CommandQueue(SimulationRuntime &runtime, std::string_view name)
        : CommandQueueBase(runtime, name) {}

    // Queues a command. From any thread, at any time; it never touches the world.
    void Push(const T &command) {
        const std::lock_guard lock(m_Mutex);
        m_Incoming.push_back(command);
        CountQueued(m_Incoming.size());
    }
    void Push(T &&command) {
        const std::lock_guard lock(m_Mutex);
        m_Incoming.push_back(std::move(command));
        CountQueued(m_Incoming.size());
    }

    // Applies every command queued so far: calls apply(const T &) for each, in the order they
    // were queued, and returns how many. A command queued while it runs waits for the next call.
    // Called by the queue's owner, from a system of the realtime slot's Commands phase.
    template <class Apply> std::size_t Drain(Apply &&apply) {
        CheckCommandsPhase();
        {
            const std::lock_guard lock(m_Mutex);
            m_Incoming.swap(m_Applying); // both keep their storage: no allocation once warm
            CountQueued(m_Incoming.size());
        }
        for (const T &command : m_Applying) {
            apply(command);
        }
        const std::size_t applied = m_Applying.size();
        m_Applying.clear();
        CountApplied(applied);
        return applied;
    }

  private:
    std::mutex m_Mutex;        // guards m_Incoming
    std::vector<T> m_Incoming; // queued, from any thread
    std::vector<T> m_Applying; // being applied, by the simulation's driving thread
};

} // namespace Runic::Simulation
