#pragma once

#include <cstddef>

#include "Assert.hpp"

// A fixed-capacity queue for allocation-free paths, such as APP-1's queued application events: the
// capacity is a template parameter and the elements live inside the object, so it never allocates.

namespace Runic::Core {

// A first-in, first-out queue of at most N elements. A full queue refuses a push (it returns false)
// and never overwrites: the caller counts the drop. Not thread-safe. Not copyable or movable in
// Stage A.
template <class T, std::size_t N> class FixedRingBuffer {
    static_assert(N > 0, "a FixedRingBuffer holds at least one element");

  public:
    FixedRingBuffer() noexcept = default;
    ~FixedRingBuffer(); // destroys the elements still queued
    FixedRingBuffer(const FixedRingBuffer &) = delete;
    FixedRingBuffer &operator=(const FixedRingBuffer &) = delete;

    bool TryPush(const T &value); // false when full
    bool TryPush(T &&value);      // false when full
    bool TryPop(T &out);          // moves the front element into out; false when empty

    T &Front();             // asserts that the queue is not empty
    const T &Front() const; // asserts that the queue is not empty
    void Pop();             // destroys the front element; asserts that the queue is not empty
    void Clear() noexcept;  // destroys every element

    std::size_t Size() const noexcept;
    static constexpr std::size_t Capacity() noexcept { return N; }
    bool IsEmpty() const noexcept;
    bool IsFull() const noexcept;

  private:
    alignas(T) std::byte m_Storage[sizeof(T) * N];
    std::size_t m_Head = 0; // the slot of the front element
    std::size_t m_Size = 0;
};

// CORE-1's implementation session defines the members here.

} // namespace Runic::Core
