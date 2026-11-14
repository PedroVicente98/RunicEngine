#pragma once

#include <cstddef>

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
    alignas(T) std::byte m_Storage[sizeof(T) * N];
    std::size_t m_Size = 0;
};

// CORE-1's implementation session defines the members here.

} // namespace Runic::Core
