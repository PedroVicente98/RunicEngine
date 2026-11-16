// The per-tick arena (TickArena.hpp): a chain of blocks filled front to back.

#include "TickArena.hpp"

#include <bit>
#include <cstdint>

#include "Assert.hpp"

namespace Runic::Core {

// A block's header, followed in the same allocation by its storage.
struct TickArena::Block {
    Block *next = nullptr;
    std::size_t bytes = 0; // the storage after the header
    std::size_t used = 0;

    std::byte *Storage() noexcept { return reinterpret_cast<std::byte *>(this + 1); }
};

namespace {

// The alignment of a block's allocation, so storage for any ordinary type starts aligned.
constexpr std::align_val_t kBlockAlignment{alignof(std::max_align_t)};

// The offset in a block where an allocation of this alignment may start, at or after used.
std::size_t AlignedOffset(const std::byte *storage, std::size_t used, std::size_t alignment) {
    const auto address = reinterpret_cast<std::uintptr_t>(storage) + used;
    const std::uintptr_t aligned = (address + alignment - 1) & ~(alignment - 1);
    return used + static_cast<std::size_t>(aligned - address);
}

} // namespace

TickArena::TickArena(std::size_t blockBytes) noexcept : m_BlockBytes(blockBytes) {
    RUNIC_ASSERT(blockBytes > 0);
}

TickArena::~TickArena() {
    for (Block *block = m_First; block != nullptr;) {
        Block *next = block->next;
        ::operator delete(block, kBlockAlignment);
        block = next;
    }
}

void *TickArena::Allocate(std::size_t bytes, std::size_t alignment) {
    RUNIC_ASSERT(std::has_single_bit(alignment));

    // The current block first, then the free blocks after it, in order.
    Block *last = nullptr;
    for (Block *block = m_Current != nullptr ? m_Current : m_First; block != nullptr;
         block = block->next) {
        const std::size_t start = AlignedOffset(block->Storage(), block->used, alignment);
        if (start <= block->bytes && bytes <= block->bytes - start) {
            m_UsedBytes += start - block->used + bytes;
            block->used = start + bytes;
            m_Current = block;
            return block->Storage() + start;
        }
        last = block;
    }

    // None fits: one more block at the end of the chain, large enough for this request whatever
    // its alignment.
    const std::size_t storage = bytes + alignment > m_BlockBytes ? bytes + alignment : m_BlockBytes;
    auto *block = ::new (::operator new(sizeof(Block) + storage, kBlockAlignment)) Block;
    block->bytes = storage;
    if (last != nullptr) {
        last->next = block; // the loop above ended on the chain's last block
    } else {
        m_First = block;
    }
    m_ReservedBytes += storage;
    ++m_BlockCount;

    const std::size_t start = AlignedOffset(block->Storage(), block->used, alignment);
    m_UsedBytes += start + bytes;
    block->used = start + bytes;
    m_Current = block;
    return block->Storage() + start;
}

void TickArena::Reset() noexcept {
    for (Block *block = m_First; block != nullptr; block = block->next) {
        block->used = 0;
    }
    m_Current = m_First;
    m_UsedBytes = 0;
}

} // namespace Runic::Core
