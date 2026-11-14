#pragma once

#include <cstddef>
#include <new>
#include <utility>

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
    T *Slot(std::size_t index) noexcept {
        return std::launder(reinterpret_cast<T *>(m_Storage + index * sizeof(T)));
    }
    const T *Slot(std::size_t index) const noexcept {
        return std::launder(reinterpret_cast<const T *>(m_Storage + index * sizeof(T)));
    }

    alignas(T) std::byte m_Storage[sizeof(T) * N];
    std::size_t m_Head = 0; // the slot of the front element
    std::size_t m_Size = 0;
};

template <class T, std::size_t N> FixedRingBuffer<T, N>::~FixedRingBuffer() { Clear(); }

template <class T, std::size_t N> bool FixedRingBuffer<T, N>::TryPush(const T &value) {
    if (IsFull()) {
        return false;
    }
    ::new (static_cast<void *>(m_Storage + ((m_Head + m_Size) % N) * sizeof(T))) T(value);
    ++m_Size;
    return true;
}

template <class T, std::size_t N> bool FixedRingBuffer<T, N>::TryPush(T &&value) {
    if (IsFull()) {
        return false;
    }
    ::new (static_cast<void *>(m_Storage + ((m_Head + m_Size) % N) * sizeof(T)))
        T(std::move(value));
    ++m_Size;
    return true;
}

template <class T, std::size_t N> bool FixedRingBuffer<T, N>::TryPop(T &out) {
    if (IsEmpty()) {
        return false;
    }
    out = std::move(*Slot(m_Head));
    Pop();
    return true;
}

template <class T, std::size_t N> T &FixedRingBuffer<T, N>::Front() {
    RUNIC_ASSERT_MSG(!IsEmpty(), "FixedRingBuffer::Front() on an empty buffer");
    return *Slot(m_Head);
}

template <class T, std::size_t N> const T &FixedRingBuffer<T, N>::Front() const {
    RUNIC_ASSERT_MSG(!IsEmpty(), "FixedRingBuffer::Front() on an empty buffer");
    return *Slot(m_Head);
}

template <class T, std::size_t N> void FixedRingBuffer<T, N>::Pop() {
    RUNIC_ASSERT_MSG(!IsEmpty(), "FixedRingBuffer::Pop() on an empty buffer");
    Slot(m_Head)->~T();
    m_Head = (m_Head + 1) % N;
    --m_Size;
}

template <class T, std::size_t N> void FixedRingBuffer<T, N>::Clear() noexcept {
    while (m_Size > 0) {
        Slot(m_Head)->~T();
        m_Head = (m_Head + 1) % N;
        --m_Size;
    }
    m_Head = 0;
}

template <class T, std::size_t N> std::size_t FixedRingBuffer<T, N>::Size() const noexcept {
    return m_Size;
}
template <class T, std::size_t N> bool FixedRingBuffer<T, N>::IsEmpty() const noexcept {
    return m_Size == 0;
}
template <class T, std::size_t N> bool FixedRingBuffer<T, N>::IsFull() const noexcept {
    return m_Size == N;
}

} // namespace Runic::Core
