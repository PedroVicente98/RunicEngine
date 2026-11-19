#include "Result.hpp"

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <utility>

using namespace Runic::Core;

TEST(Result, HoldsAValue) {
    const Result<int> result = 42;
    EXPECT_TRUE(result.HasValue());
    EXPECT_TRUE(static_cast<bool>(result));
    EXPECT_EQ(result.Value(), 42);
}

TEST(Result, HoldsAnErrorWithItsMessage) {
    const Result<int> result = Fail("no layer named 'engine:probe_a'");
    EXPECT_FALSE(result.HasValue());
    EXPECT_FALSE(static_cast<bool>(result));
    EXPECT_EQ(result.GetError().message, "no layer named 'engine:probe_a'");
}

TEST(Result, VoidSucceedsOrFails) {
    const Result<void> success = {};
    EXPECT_TRUE(success.HasValue());
    const Result<void> failure = Fail("libgodot is missing");
    EXPECT_FALSE(failure.HasValue());
    EXPECT_EQ(failure.GetError().message, "libgodot is missing");
}

TEST(Result, MovesItsValueOut) {
    Result<std::unique_ptr<int>> result = std::make_unique<int>(7);
    const std::unique_ptr<int> value = std::move(result).Value();
    ASSERT_NE(value, nullptr);
    EXPECT_EQ(*value, 7);
}

#if RUNIC_ASSERTS_ENABLED
TEST(ResultDeathTest, ValueOfAnErrorAsserts) {
    const Result<int> result = Fail("refused");
    EXPECT_DEATH(static_cast<void>(result.Value()),
                 "assert failed.*Result::Value\\(\\) on an error");
}

TEST(ResultDeathTest, ErrorOfASuccessAsserts) {
    const Result<void> result = {};
    EXPECT_DEATH(static_cast<void>(result.GetError()),
                 "assert failed.*Result::GetError\\(\\) on a success");
}
#endif
