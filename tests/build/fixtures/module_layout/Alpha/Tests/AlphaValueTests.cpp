#include "AlphaValue.hpp"

#include <gtest/gtest.h>

TEST(AlphaValue, Answer) { EXPECT_EQ(Fixture::Alpha::Answer(), 42); }
