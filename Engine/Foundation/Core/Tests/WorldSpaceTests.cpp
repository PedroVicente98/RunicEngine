#include "WorldSpace.hpp"

#include <gtest/gtest.h>

#include <type_traits>

using namespace Runic::Core;

namespace {
// Concepts, because a bare requires-expression outside a template is a hard error rather than
// false.
template <class A, class B>
concept CanAdd = requires(A a, B b) { a + b; };
template <class A, class B>
concept CanSubtract = requires(A a, B b) { a - b; };
template <class A, class B>
concept CanMultiply = requires(A a, B b) { a * b; };
} // namespace

// The type rule, checked at compile time (CORE-1: adding two WorldPositions or converting one to a
// float vector does not compile).
static_assert(!CanAdd<WorldPosition, WorldPosition>, "two places do not add");
static_assert(!CanMultiply<WorldPosition, double> && !CanMultiply<double, WorldPosition>,
              "a place does not scale");
static_assert(!std::is_convertible_v<WorldPosition, Vec3> &&
              !std::is_constructible_v<Vec3, WorldPosition>);
static_assert(!std::is_convertible_v<WorldDelta, Vec3> &&
              !std::is_constructible_v<Vec3, WorldDelta>);
static_assert(!std::is_convertible_v<glm::dvec3, Vec3>,
              "GLM_FORCE_EXPLICIT_CTOR: no implicit 64 to 32 bits");
static_assert(std::is_constructible_v<Vec3, glm::dvec3>, "an explicit conversion stays possible");

// What is defined.
static_assert(CanSubtract<WorldPosition, WorldPosition>);
static_assert(CanAdd<WorldPosition, WorldDelta> && CanSubtract<WorldPosition, WorldDelta>);
static_assert(CanAdd<WorldDelta, WorldDelta> && CanSubtract<WorldDelta, WorldDelta>);
static_assert(CanMultiply<WorldDelta, double>);
static_assert(CanAdd<WorldPosition, Vec3>, "a 32-bit offset goes up");
static_assert(WorldPosition{3, 4, 5} - WorldPosition{1, 1, 1} == WorldDelta{2, 3, 4});
static_assert(DistanceSq(WorldPosition{0, 0, 0}, WorldPosition{3, 4, 0}) == 25.0);

TEST(WorldSpace, ArithmeticTenThousandKilometresOut) {
    const WorldPosition far{1.0e7, -2'000.0, 1.0e7}; // 10,000 km from the origin, 2 km down
    const WorldDelta step{0.01, -0.01, 0.01};        // 1 cm, the smallest unit gameplay relies on
    const WorldDelta back = (far + step) - far;
    EXPECT_NEAR(back.x, 0.01, 1e-6); // within a micrometre
    EXPECT_NEAR(back.y, -0.01, 1e-6);
    EXPECT_NEAR(back.z, 0.01, 1e-6);
    const WorldPosition returned = (far + step) - step;
    EXPECT_NEAR(returned.x, far.x, 1e-6);
    EXPECT_NEAR(returned.z, far.z, 1e-6);
}

TEST(WorldSpace, DeltasAddSubtractAndScale) {
    const WorldDelta a{1.0, 2.0, 3.0};
    const WorldDelta b{0.5, -1.0, 2.0};
    EXPECT_EQ(a + b, (WorldDelta{1.5, 1.0, 5.0}));
    EXPECT_EQ(a - b, (WorldDelta{0.5, 3.0, 1.0}));
    EXPECT_EQ(a * 2.0, (WorldDelta{2.0, 4.0, 6.0}));
    EXPECT_EQ(WorldPosition{} + a - a, WorldPosition{});
}

TEST(WorldSpace, LocalOffsetGoesUpWithNoLoss) {
    const WorldPosition p{1.0e6, 0.0, -3.0e6};
    EXPECT_EQ(p + Vec3(1.5f, -2.25f, 0.125f), (WorldPosition{1.0e6 + 1.5, -2.25, -3.0e6 + 0.125}));
}

TEST(WorldSpace, ToLocalUnderTenKilometres) {
    EXPECT_EQ(ToLocal(WorldDelta{1234.5, -10.0, 0.25}), Vec3(1234.5f, -10.0f, 0.25f));
    EXPECT_EQ(ToLocal(WorldDelta{9'999.0, 0.0, 0.0}), Vec3(9'999.0f, 0.0f, 0.0f));
}

#if RUNIC_ASSERTS_ENABLED
TEST(WorldSpaceDeathTest, ToLocalRefusesTenKilometresOrMore) {
    EXPECT_DEATH(static_cast<void>(ToLocal(WorldDelta{kMaxLocalOffsetM, 0.0, 0.0})),
                 "assert failed");
    EXPECT_DEATH(static_cast<void>(ToLocal(WorldDelta{8'000.0, 8'000.0, 0.0})), "assert failed");
}
#endif
