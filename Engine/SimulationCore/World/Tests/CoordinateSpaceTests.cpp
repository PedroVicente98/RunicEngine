#include "CoordinateSpace.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <limits>
#include <random>
#include <unordered_set>

#include "CellKeys.hpp"

using namespace Runic;
using namespace Runic::World;

namespace {

// A world whose grid does not start at zero and whose cells are not a power of two.
CoordinateSpace OddSpace() {
    CoordinateSpace space;
    space.origin = Core::WorldPosition{-1'234.0, 77.0, 9'001.0};
    space.cellSizeM = 50;
    space.cellsPerSector = 6;
    return space;
}

} // namespace

TEST(CoordinateSpace, APositionFallsInExactlyOneCell) {
    const CoordinateSpace space; // 64 m cells from the origin
    EXPECT_EQ(space.CellOf({0.0, 0.0, 0.0}), (CellCoord{0, 0, 0}));
    EXPECT_EQ(space.CellOf({63.999, 63.999, 63.999}), (CellCoord{0, 0, 0}));
    EXPECT_EQ(space.CellOf({64.0, 0.0, 0.0}),
              (CellCoord{1, 0, 0})); // a boundary is the next cell's
    EXPECT_EQ(space.CellOf({-0.001, 0.0, 0.0}), (CellCoord{-1, 0, 0})); // floor, not truncation
    EXPECT_EQ(space.CellOf({-64.0, -64.0, -64.0}), (CellCoord{-1, -1, -1}));
    EXPECT_EQ(space.CellOf({-64.001, 0.0, 128.0}), (CellCoord{-2, 0, 2}));
    // The last 64-bit value below a boundary is still the lower cell.
    EXPECT_EQ(space.CellOf({std::nextafter(6'400'000.0, 0.0), 0.0, 0.0}),
              (CellCoord{99'999, 0, 0}));
    EXPECT_EQ(space.CellOf({6'400'000.0, 0.0, 0.0}), (CellCoord{100'000, 0, 0}));
}

TEST(CoordinateSpace, ACellKeyRoundTripsAtAnyAltitudeOrDepth) {
    for (const CoordinateSpace &space : {CoordinateSpace(), OddSpace()}) {
        // Under the sea floor, at the surface, above the clouds, and at the ends of the range.
        const std::int32_t heights[] = {std::numeric_limits<std::int32_t>::min(),
                                        -2'000'000,
                                        -200,
                                        -1,
                                        0,
                                        1,
                                        47,
                                        3'000,
                                        2'000'000,
                                        std::numeric_limits<std::int32_t>::max()};
        for (const std::int32_t y : heights) {
            for (const std::int32_t x : {-93'750, 0, 93'750}) { // ± 6,000 km of 64 m cells
                const CellCoord cell{x, y, -x};
                ASSERT_EQ(space.CellOf(space.CellMin(cell)), cell) << "y = " << y;
                ASSERT_EQ(space.CellOf(space.CellCenter(cell)), cell) << "y = " << y;
            }
        }
    }

    // And for cells anywhere in the 32-bit range.
    std::mt19937 random(2026);
    std::uniform_int_distribution<std::int32_t> any(std::numeric_limits<std::int32_t>::min(),
                                                    std::numeric_limits<std::int32_t>::max());
    const CoordinateSpace space = OddSpace();
    for (int round = 0; round < 100'000; ++round) {
        const CellCoord cell{any(random), any(random), any(random)};
        ASSERT_EQ(space.CellOf(space.CellMin(cell)), cell);
        ASSERT_EQ(space.CellOf(space.CellCenter(cell)), cell);
    }
}

TEST(CoordinateSpace, EveryPointOfACellMapsToItFarFromTheOrigin) {
    const CoordinateSpace space = OddSpace();
    const CellCoord cell{120'000, -64, -150'000}; // 6,000 km east, 3 km down, 7,500 km south
    const Core::WorldPosition min = space.CellMin(cell);
    EXPECT_DOUBLE_EQ(min.x, -1'234.0 + 120'000.0 * 50.0);
    EXPECT_DOUBLE_EQ(min.y, 77.0 - 64.0 * 50.0);

    std::mt19937 random(7);
    std::uniform_real_distribution<double> inside(0.0, 50.0);
    for (int round = 0; round < 100'000; ++round) {
        const Core::WorldPosition point{min.x + inside(random), min.y + inside(random),
                                        min.z + inside(random)};
        if (point.x < min.x + 50.0 && point.y < min.y + 50.0 && point.z < min.z + 50.0) {
            ASSERT_EQ(space.CellOf(point), cell);
        }
    }
    // The far corner belongs to the next cell on every axis.
    EXPECT_EQ(space.CellOf({min.x + 50.0, min.y + 50.0, min.z + 50.0}),
              (CellCoord{cell.x + 1, cell.y + 1, cell.z + 1}));
}

TEST(CoordinateSpace, TheOffsetInACellRebuildsThePosition) {
    const CoordinateSpace space;
    std::mt19937 random(11);
    std::uniform_real_distribution<double> far(-7'000'000.0, 7'000'000.0);
    std::uniform_real_distribution<double> height(-11'000.0, 9'000.0);
    constexpr double kFourMicrometres = 4e-6; // the step of a float below 64
    for (int round = 0; round < 100'000; ++round) {
        const Core::WorldPosition position{far(random), height(random), far(random)};
        const CellCoord cell = space.CellOf(position);
        const Core::Vec3 offset = space.OffsetInCell(position);
        ASSERT_GE(offset.x, 0.0f);
        ASSERT_LE(offset.x, 64.0f);
        ASSERT_GE(offset.y, 0.0f);
        ASSERT_GE(offset.z, 0.0f);
        const Core::WorldPosition rebuilt = space.CellMin(cell) + offset;
        ASSERT_NEAR(rebuilt.x, position.x, kFourMicrometres);
        ASSERT_NEAR(rebuilt.y, position.y, kFourMicrometres);
        ASSERT_NEAR(rebuilt.z, position.z, kFourMicrometres);
    }
}

TEST(CoordinateSpace, SectorsAreBucketsOfCellsOnTheGroundPlane) {
    const CoordinateSpace space; // 8 cells of 64 m: 512 m sectors
    EXPECT_DOUBLE_EQ(space.SectorSizeM(), 512.0);
    EXPECT_EQ(space.SectorOf(CellCoord{0, 0, 0}), (SectorCoord{0, 0}));
    EXPECT_EQ(space.SectorOf(CellCoord{7, 500, 7}), (SectorCoord{0, 0})); // y does not matter
    EXPECT_EQ(space.SectorOf(CellCoord{8, -500, 15}), (SectorCoord{1, 1}));
    EXPECT_EQ(space.SectorOf(CellCoord{-1, 0, -8}), (SectorCoord{-1, -1})); // floor
    EXPECT_EQ(space.SectorOf(CellCoord{-9, 0, -16}), (SectorCoord{-2, -2}));
    EXPECT_EQ(space.SectorOf(Core::WorldPosition{511.9, 1'000.0, -0.1}), (SectorCoord{0, -1}));
    EXPECT_EQ(space.SectorOf(Core::WorldPosition{512.0, -1'000.0, -512.1}), (SectorCoord{1, -2}));

    const CoordinateSpace odd = OddSpace(); // 6 cells of 50 m from (-1234, 77, 9001)
    EXPECT_DOUBLE_EQ(odd.SectorSizeM(), 300.0);
    EXPECT_EQ(odd.SectorOf(Core::WorldPosition{-1'234.0, 0.0, 9'001.0}), (SectorCoord{0, 0}));
    EXPECT_EQ(odd.SectorOf(Core::WorldPosition{-1'234.1, 0.0, 9'301.0}), (SectorCoord{-1, 1}));
}

TEST(CellKeys, AreTheirWorldAndTheirAddress) {
    const Core::WorldId main(1);
    const Core::WorldId other(2);
    const CellKey ground{main, {10, 0, -4}};
    const CellKey cave{main, {10, -3, -4}}; // under the same ground cell
    const CellKey elsewhere{other, {10, 0, -4}};
    EXPECT_EQ(ground, (CellKey{main, {10, 0, -4}}));
    EXPECT_NE(ground, cave);
    EXPECT_NE(ground, elsewhere);
    EXPECT_NE((SectorKey{main, {1, 2}}), (SectorKey{main, {2, 1}}));
    EXPECT_NE((SectorKey{main, {1, 2}}), (SectorKey{other, {1, 2}}));

    // Usable as keys of a sparse map: a column of cells, and the same cells of another world.
    std::unordered_set<CellKey> cells;
    for (std::int32_t y = -500; y <= 500; ++y) {
        cells.insert(CellKey{main, {3, y, 3}});
        cells.insert(CellKey{other, {3, y, 3}});
    }
    EXPECT_EQ(cells.size(), 2'002u);
    EXPECT_TRUE(cells.contains(CellKey{main, {3, -500, 3}}));
    EXPECT_FALSE(cells.contains(CellKey{main, {3, 501, 3}}));
    std::unordered_set<SectorKey> sectors{SectorKey{main, {0, 0}}, SectorKey{main, {0, 1}},
                                          SectorKey{other, {0, 0}}, SectorKey{main, {0, 0}}};
    EXPECT_EQ(sectors.size(), 3u);
}
