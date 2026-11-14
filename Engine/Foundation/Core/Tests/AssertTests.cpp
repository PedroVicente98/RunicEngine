#include "Assert.hpp"

#include <gtest/gtest.h>

#include <cstdio>
#include <cstdlib>

using namespace Runic::Core;

TEST(Assert, CompiledOutAssertsNeverEvaluate) {
    int evaluated = 0;
    RUNIC_ASSERT((++evaluated, true));
    EXPECT_EQ(evaluated, RUNIC_ASSERTS_ENABLED ? 1 : 0);
}

#if RUNIC_ASSERTS_ENABLED
TEST(AssertDeathTest, FailurePrintsTheConditionAndAborts) {
    EXPECT_DEATH(RUNIC_ASSERT(1 + 1 == 3), "assert failed: 1 \\+ 1 == 3");
}

TEST(AssertDeathTest, MessageAndLocationArePrinted) {
    EXPECT_DEATH(RUNIC_ASSERT_MSG(false, "the frame must be open"),
                 "assert failed: false - the frame must be open \\(.*AssertTests.cpp:[0-9]+\\)");
}

namespace {
[[noreturn]] void MarkerHandler(const AssertFailure &) {
    std::fputs("RUNIC-TEST: replaced handler\n", stderr);
    std::abort();
}
} // namespace

TEST(AssertDeathTest, ReplacedHandlerRuns) {
    EXPECT_DEATH(
        {
            SetAssertHandler(&MarkerHandler);
            RUNIC_ASSERT(false);
        },
        "RUNIC-TEST: replaced handler");
}
#endif
