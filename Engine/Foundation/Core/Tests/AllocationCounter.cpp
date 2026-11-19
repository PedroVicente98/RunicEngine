// The test executable's replacement of the global operator new and delete, which counts allocations
// for AllocationScope. It belongs to the tests, not to Runic::Core.

#include "AllocationCounter.hpp"

#include <cstdlib>
#include <new>

namespace {

thread_local int t_ActiveScopes = 0;
thread_local std::size_t t_Allocations = 0;

void CountAllocation() noexcept {
    if (t_ActiveScopes > 0) {
        ++t_Allocations;
    }
}

void *Allocate(std::size_t size) {
    CountAllocation();
    if (void *memory = std::malloc(size == 0 ? 1 : size)) {
        return memory;
    }
    throw std::bad_alloc();
}

void *AllocateAligned(std::size_t size, std::align_val_t alignment) {
    CountAllocation();
    const auto align = static_cast<std::size_t>(alignment);
    const std::size_t rounded = size == 0 ? align : (size + align - 1) / align * align;
#if defined(_WIN32)
    void *memory = _aligned_malloc(rounded, align);
#else
    void *memory = std::aligned_alloc(align, rounded);
#endif
    if (memory != nullptr) {
        return memory;
    }
    throw std::bad_alloc();
}

void FreeAligned(void *memory) noexcept {
#if defined(_WIN32)
    _aligned_free(memory);
#else
    std::free(memory);
#endif
}

} // namespace

void *operator new(std::size_t size) { return Allocate(size); }
void *operator new[](std::size_t size) { return Allocate(size); }
void *operator new(std::size_t size, const std::nothrow_t &) noexcept {
    try {
        return Allocate(size);
    } catch (...) {
        return nullptr;
    }
}
void *operator new[](std::size_t size, const std::nothrow_t &) noexcept {
    try {
        return Allocate(size);
    } catch (...) {
        return nullptr;
    }
}
void *operator new(std::size_t size, std::align_val_t alignment) {
    return AllocateAligned(size, alignment);
}
void *operator new[](std::size_t size, std::align_val_t alignment) {
    return AllocateAligned(size, alignment);
}

void operator delete(void *memory) noexcept { std::free(memory); }
void operator delete[](void *memory) noexcept { std::free(memory); }
void operator delete(void *memory, std::size_t) noexcept { std::free(memory); }
void operator delete[](void *memory, std::size_t) noexcept { std::free(memory); }
void operator delete(void *memory, std::align_val_t) noexcept { FreeAligned(memory); }
void operator delete[](void *memory, std::align_val_t) noexcept { FreeAligned(memory); }
void operator delete(void *memory, std::size_t, std::align_val_t) noexcept { FreeAligned(memory); }
void operator delete[](void *memory, std::size_t, std::align_val_t) noexcept {
    FreeAligned(memory);
}

namespace Runic::Core::Testing {

AllocationScope::AllocationScope() noexcept : m_Start(t_Allocations) { ++t_ActiveScopes; }
AllocationScope::~AllocationScope() { --t_ActiveScopes; }
std::size_t AllocationScope::Count() const noexcept { return t_Allocations - m_Start; }

} // namespace Runic::Core::Testing
