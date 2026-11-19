// Asserts (Assert.hpp): the failure is logged at Fatal, then the handler runs; the process never
// continues.

#include "Assert.hpp"

#include "Log.hpp"

#include <atomic>
#include <cstdlib>

namespace Runic::Core {

namespace {
std::atomic<AssertHandler> g_Handler{nullptr};
} // namespace

void SetAssertHandler(AssertHandler handler) noexcept {
    g_Handler.store(handler, std::memory_order_release);
}

void FailAssert(const AssertFailure &failure) noexcept {
    if (failure.message != nullptr) {
        Log(LogLevel::Fatal, failure.file, failure.line, "assert failed: {} - {} ({}:{})",
            failure.condition, failure.message, failure.file, failure.line);
    } else {
        Log(LogLevel::Fatal, failure.file, failure.line, "assert failed: {} ({}:{})",
            failure.condition, failure.file, failure.line);
    }
    if (const AssertHandler handler = g_Handler.load(std::memory_order_acquire)) {
        handler(failure);
    }
    std::abort();
}

} // namespace Runic::Core
