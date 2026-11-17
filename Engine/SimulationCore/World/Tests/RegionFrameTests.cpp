#include "RegionFrame.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <random>

using namespace Runic;
using namespace Runic::World;

namespace {

// Region-local coordinates the way content has them: zero, or at least 1 cm, either sign.
class LocalValues {
  public:
    float Next() {
        const float magnitude = std::exp2(m_Exponent(m_Random)) * m_Mantissa(m_Random);
        if (magnitude < 0.01f) {
            return 0.0f;
        }
        return m_Sign(m_Random) == 0 ? magnitude : -magnitude;
    }

  private:
    std::mt19937 m_Random{42};
    // 4 mm to 4 km before the cut, so three of them stay inside ToLocal's 10 km.
    std::uniform_real_distribution<float> m_Exponent{-8.0f, 11.0f};
    std::uniform_real_distribution<float> m_Mantissa{1.0f, 2.0f};
    std::uniform_int_distribution<int> m_Sign{0, 1};
};

} // namespace

TEST(RegionFrame, ALocalPositionGoesUpToTheWorldAndBackUnchangedFarFromTheOrigin) {
    // Regions thousands of kilometres out, deep and high, with origins that are not round numbers.
    const RegionFrame frames[] = {
        {{0.0, 0.0, 0.0}},
        {{6'000'000.5, -3'200.25, -7'500'000.125}},
        {{-4'321'987.654321, 8'848.86, 1'234'567.891011}},
        {{kExactRegionRangeM, -kExactRegionRangeM,
          kExactRegionRangeM}}, // the end of the exact range
    };
    LocalValues values;
    for (const RegionFrame &frame : frames) {
        for (int round = 0; round < 100'000; ++round) {
            const Core::Vec3 local(values.Next(), values.Next(), values.Next());
            const Core::Vec3 back = frame.ToRegionLocal(frame.ToWorld(local));
            // Bit for bit, not "close": content does not shift when its region is far away.
            ASSERT_EQ(back.x, local.x) << "origin x " << frame.origin.x;
            ASSERT_EQ(back.y, local.y) << "origin y " << frame.origin.y;
            ASSERT_EQ(back.z, local.z) << "origin z " << frame.origin.z;
        }
    }
}

TEST(RegionFrame, AWorldPositionComesBackWithinAFloatStep) {
    const RegionFrame frame{{6'000'000.5, -3'200.25, -7'500'000.125}};
    std::mt19937 random(5);
    std::uniform_real_distribution<double> near(-5'000.0, 5'000.0);
    constexpr double kHalfAMillimetre = 0.0005; // a float's step below 8 km is under 1 mm
    for (int round = 0; round < 100'000; ++round) {
        const Core::WorldPosition position{frame.origin.x + near(random),
                                           frame.origin.y + near(random),
                                           frame.origin.z + near(random)};
        ASSERT_TRUE(frame.Reaches(position));
        const Core::WorldPosition back = frame.ToWorld(frame.ToRegionLocal(position));
        ASSERT_NEAR(back.x, position.x, kHalfAMillimetre);
        ASSERT_NEAR(back.y, position.y, kHalfAMillimetre);
        ASSERT_NEAR(back.z, position.z, kHalfAMillimetre);
    }
}

TEST(RegionFrame, GoingUpIsAnAdditionInSixtyFourBits) {
    const RegionFrame frame{{6'000'000.0, 100.0, -6'000'000.0}};
    const Core::WorldPosition place = frame.ToWorld(Core::Vec3(0.01f, -0.25f, 1'000.5f));
    // One centimetre is still there six thousand kilometres out.
    EXPECT_DOUBLE_EQ(place.x, 6'000'000.0 + static_cast<double>(0.01f));
    EXPECT_DOUBLE_EQ(place.y, 99.75);
    EXPECT_DOUBLE_EQ(place.z, -5'998'999.5);
    EXPECT_NE(place.x, 6'000'000.0);
}

TEST(RegionFrame, ReachesTellsWhetherAPlaceHasALocalPosition) {
    const RegionFrame frame{{1'000'000.0, 0.0, 1'000'000.0}};
    EXPECT_TRUE(frame.Reaches(frame.origin));
    EXPECT_TRUE(frame.Reaches({1'009'999.0, 0.0, 1'000'000.0}));
    EXPECT_FALSE(frame.Reaches({1'010'000.0, 0.0, 1'000'000.0}));     // 10 km: too far for a float
    EXPECT_FALSE(frame.Reaches({1'007'100.0, 7'100.0, 1'000'000.0})); // the distance, not each axis
    EXPECT_FALSE(frame.Reaches({0.0, 0.0, 0.0}));
}

TEST(RegionFrame, BeyondTheExactRangeARoundTripIsOffByNanometres) {
    const RegionFrame frame{{12'000'000.25, 0.0, -15'000'000.75}}; // 12,000 and 15,000 km out
    LocalValues values;
    constexpr double kTwoNanometres = 2e-9;
    for (int round = 0; round < 100'000; ++round) {
        const Core::Vec3 local(values.Next(), 0.0f, values.Next());
        const Core::Vec3 back = frame.ToRegionLocal(frame.ToWorld(local));
        ASSERT_NEAR(static_cast<double>(back.x), static_cast<double>(local.x), kTwoNanometres);
        ASSERT_NEAR(static_cast<double>(back.z), static_cast<double>(local.z), kTwoNanometres);
    }
}
