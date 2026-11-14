#pragma once

#include <cstddef>
#include <new>
#include <utility>

#include "Assert.hpp"

// A fixed-capacity vector for allocation-free paths: the capacity is a template parameter and the
// elements live inside the object, so it never allocates.

namespace Runic::Core {

// A vector of at most N elements, kept in insertion order. A full vector refuses a push (it returns
// false). Not thread-safe. Not copyable or movable in Stage A.
template <class T, std::size_t N> class FixedVector {
    static_assert(N > 0, "a FixedVector holds at least one element");

  public:
    FixedVector() noexcept = default;
    ~FixedVector(); // destroys the elements
    FixedVector(const FixedVector &) = delete;
    FixedVector &operator=(const FixedVector &) = delete;

    bool TryPushBack(const T &value); // false when full
    bool TryPushBack(T &&value);      // false when full
    void PopBack();        // destroys the last element; asserts that the vector is not empty
    void Clear() noexcept; // destroys every element

    T &operator[](std::size_t index);             // asserts index < Size()
    const T &operator[](std::size_t index) const; // asserts index < Size()

    // Iteration in insertion order (lower case, for range-based for).
    T *begin() noexcept;
    T *end() noexcept;
    const T *begin() const noexcept;
    const T *end() const noexcept;

    std::size_t Size() const noexcept;
    static constexpr std::size_t Capacity() noexcept { return N; }
    bool IsEmpty() const noexcept;
    bool IsFull() const noexcept;

  private:
    T *Data() noexcept { return std::launder(reinterpret_cast<T *>(m_Storage)); }
    const T *Data() const noexcept { return std::launder(reinterpret_cast<const T *>(m_Storage)); }

    alignas(T) std::byte m_Storage[sizeof(T) * N];
    std::size_t m_Size = 0;
};

template <class T, std::size_t N> FixedVector<T, N>::~FixedVector() { Clear(); }

template <class T, std::size_t N> bool FixedVector<T, N>::TryPushBack(const T &value) {
    if (IsFull()) {
        return false;
    }
    ::new (static_cast<void *>(m_Storage + m_Size * sizeof(T))) T(value);
    ++m_Size;
    return true;
}

template <class T, std::size_t N> bool FixedVector<T, N>::TryPushBack(T &&value) {
    if (IsFull()) {
        return false;
    }
    ::new (static_cast<void *>(m_Storage + m_Size * sizeof(T))) T(std::move(value));
    ++m_Size;
    return true;
}

template <class T, std::size_t N> void FixedVector<T, N>::PopBack() {
    RUNIC_ASSERT_MSG(!IsEmpty(), "FixedVector::PopBack() on an empty vector");
    --m_Size;
    Data()[m_Size].~T();
}

template <class T, std::size_t N> void FixedVector<T, N>::Clear() noexcept {
    while (m_Size > 0) {
        --m_Size;
        Data()[m_Size].~T();
    }
}

template <class T, std::size_t N> T &FixedVector<T, N>::operator[](std::size_t index) {
    RUNIC_ASSERT_MSG(index < m_Size, "FixedVector: index past the end");
    return Data()[index];
}

template <class T, std::size_t N> const T &FixedVector<T, N>::operator[](std::size_t index) const {
    RUNIC_ASSERT_MSG(index < m_Size, "FixedVector: index past the end");
    return Data()[index];
}

template <class T, std::size_t N> T *FixedVector<T, N>::begin() noexcept { return Data(); }
template <class T, std::size_t N> T *FixedVector<T, N>::end() noexcept { return Data() + m_Size; }
template <class T, std::size_t N> const T *FixedVector<T, N>::begin() const noexcept {
    return Data();
}
template <class T, std::size_t N> const T *FixedVector<T, N>::end() const noexcept {
    return Data() + m_Size;
}
template <class T, std::size_t N> std::size_t FixedVector<T, N>::Size() const noexcept {
    return m_Size;
}
template <class T, std::size_t N> bool FixedVector<T, N>::IsEmpty() const noexcept {
    return m_Size == 0;
}
template <class T, std::size_t N> bool FixedVector<T, N>::IsFull() const noexcept {
    return m_Size == N;
}

} // namespace Runic::Core
