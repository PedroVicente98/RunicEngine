#pragma once

#include <cstddef>
#include <new>
#include <type_traits>
#include <utility>

// A per-tick arena: memory that lives until the end of the tick that asked for it. Allocating is a
// pointer bump; Reset takes everything back at once and keeps the blocks, so after the first ticks
// a phase allocates nothing from the system (RunicDocs › Modules, work packages › CORE-2). One
// arena has one user at a time: a parallel phase gives each worker its own (PerWorker<TickArena>).

namespace Runic::Core {

// The size of the blocks an arena takes from the system. A request that does not fit in one gets a
// block of its own size.
inline constexpr std::size_t kDefaultArenaBlockBytes = 64 * 1024;

class TickArena {
  public:
    // Takes no memory until the first allocation.
    explicit TickArena(std::size_t blockBytes = kDefaultArenaBlockBytes) noexcept;
    ~TickArena();

    TickArena(const TickArena &) = delete;
    TickArena &operator=(const TickArena &) = delete;

    // bytes of storage at the given alignment (a power of two), valid until the next Reset. Never
    // null: a system that has no memory left ends the process, as operator new does.
    void *Allocate(std::size_t bytes, std::size_t alignment);

    // An uninitialized array of count elements. T needs no destructor: Reset runs none.
    template <class T> T *AllocateArray(std::size_t count) {
        static_assert(std::is_trivially_destructible_v<T>, "an arena runs no destructors");
        return static_cast<T *>(Allocate(count * sizeof(T), alignof(T)));
    }

    // One T built in the arena. T needs no destructor: Reset runs none.
    template <class T, class... Args> T *Create(Args &&...args) {
        static_assert(std::is_trivially_destructible_v<T>, "an arena runs no destructors");
        return ::new (Allocate(sizeof(T), alignof(T))) T(std::forward<Args>(args)...);
    }

    // Takes back every allocation and keeps every block: nothing returns to the system.
    void Reset() noexcept;

    // Bytes handed out since the last Reset, alignment padding included.
    std::size_t UsedBytes() const noexcept { return m_UsedBytes; }
    // Bytes held from the system: the sum of the blocks.
    std::size_t ReservedBytes() const noexcept { return m_ReservedBytes; }
    std::size_t BlockCount() const noexcept { return m_BlockCount; }

  private:
    struct Block;

    Block *m_First = nullptr;   // the chain, in the order the blocks were taken
    Block *m_Current = nullptr; // the block being filled; the ones after it are free
    std::size_t m_BlockBytes;
    std::size_t m_UsedBytes = 0;
    std::size_t m_ReservedBytes = 0;
    std::size_t m_BlockCount = 0;
};

} // namespace Runic::Core
