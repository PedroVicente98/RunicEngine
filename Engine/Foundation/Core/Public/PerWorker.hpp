#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>

#include "Assert.hpp"
#include "WorkerPool.hpp"

// Per-worker buffers: one T per worker slot, each on its own cache lines, so the workers of a
// phase write side by side without a lock and without slowing each other down. At the barrier the
// owner reads the slots in order, which is what makes a merge independent of thread timing
// (RunicDocs › Simulation core clock and threads › Threading model).

namespace Runic::Core {

// The size a slot is aligned to: no two slots share a cache line on the CPUs the Engine targets.
inline constexpr std::size_t kCacheLineBytes = 64;

template <class T> class PerWorker {
  public:
    // One value-initialized T per slot.
    explicit PerWorker(std::uint32_t workers)
        : m_Slots(std::make_unique<Slot[]>(workers)), m_Workers(workers) {
        RUNIC_ASSERT(workers > 0);
    }

    PerWorker(const PerWorker &) = delete;
    PerWorker &operator=(const PerWorker &) = delete;

    std::uint32_t Workers() const noexcept { return m_Workers; }

    T &operator[](WorkerIndex worker) noexcept {
        RUNIC_ASSERT(worker < m_Workers);
        return m_Slots[worker].value;
    }
    const T &operator[](WorkerIndex worker) const noexcept {
        RUNIC_ASSERT(worker < m_Workers);
        return m_Slots[worker].value;
    }

  private:
    struct alignas(kCacheLineBytes) Slot {
        T value{};
    };

    std::unique_ptr<Slot[]> m_Slots;
    std::uint32_t m_Workers;
};

} // namespace Runic::Core
