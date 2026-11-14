#pragma once

// Asserts: what Runic code states must be true. They are on in Debug builds (and wherever
// RUNIC_ENABLE_ASSERTS is defined) and compiled out otherwise; a compiled-out assert never
// evaluates its condition. A failed assert is never recovered from: it logs at Fatal and ends the
// process through the assert handler.

namespace Runic::Core {

// What a failed assert reports. Every string lives for the whole run (they are string literals).
struct AssertFailure {
    const char *condition; // the expression, as written
    const char *message;   // RUNIC_ASSERT_MSG's message, or nullptr
    const char *file;
    int line;
};

// Runs after the failure is logged. It must not return; if it does, the process aborts anyway.
using AssertHandler = void (*)(const AssertFailure &failure);

// Replaces the handler; nullptr restores the default, which calls std::abort(). Call it at start,
// before other threads run (a host can flush its logs or write a crash report here).
void SetAssertHandler(AssertHandler handler) noexcept;

// Logs "assert failed: <condition>[ - <message>] (<file>:<line>)" at Fatal, then runs the handler.
// The macros call it.
[[noreturn]] void FailAssert(const AssertFailure &failure) noexcept;

} // namespace Runic::Core

#if defined(RUNIC_ENABLE_ASSERTS) || !defined(NDEBUG)
#define RUNIC_ASSERTS_ENABLED 1
#else
#define RUNIC_ASSERTS_ENABLED 0
#endif

#if RUNIC_ASSERTS_ENABLED
#define RUNIC_ASSERT(condition)                                                                    \
    ((condition) ? static_cast<void>(0)                                                            \
                 : ::Runic::Core::FailAssert({#condition, nullptr, __FILE__, __LINE__}))
#define RUNIC_ASSERT_MSG(condition, message)                                                       \
    ((condition) ? static_cast<void>(0)                                                            \
                 : ::Runic::Core::FailAssert({#condition, message, __FILE__, __LINE__}))
#else
#define RUNIC_ASSERT(condition) static_cast<void>(sizeof(!(condition)))
#define RUNIC_ASSERT_MSG(condition, message) static_cast<void>(sizeof(!(condition)))
#endif
