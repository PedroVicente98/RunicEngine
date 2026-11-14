#pragma once

#include <cstddef>

namespace Runic::Core::Testing {

// Counts the global operator new calls this thread makes while the scope is alive. RunicCoreTests
// replaces the global operator new and delete (AllocationCounter.cpp) to count them; GoogleTest's
// own allocations outside a scope are not counted, so keep assertions outside the scope.
class AllocationScope {
  public:
    AllocationScope() noexcept;
    ~AllocationScope();
    AllocationScope(const AllocationScope &) = delete;
    AllocationScope &operator=(const AllocationScope &) = delete;

    std::size_t Count() const noexcept;

  private:
    std::size_t m_Start;
};

} // namespace Runic::Core::Testing
