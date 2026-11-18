// WORLD-2: the sparse 3D spatial index. Columns hold only occupied Y cells; queries are exact in
// 3D and return what testing every member would, each member once; membership follows moves.

#include "SpatialIndex.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <random>
#include <set>
#include <thread>
#include <tuple>
#include <vector>

#include "CoordinateSpace.hpp"

using namespace Runic;
using namespace Runic::World;
using Core::Vec3;
using Core::WorldPosition;

namespace {

// Thousands of kilometres out, where a float's step is half a metre: the index works in 64-bit.
const WorldPosition kFarAway{6'000'000.0, 0.0, -5'000'000.0};

WorldPosition At(double x, double y, double z) {
    return {kFarAway.x + x, kFarAway.y + y, kFarAway.z + z};
}

ECS::Entity Member(std::uint64_t n) { return ECS::Entity::FromRaw(n); }

struct Placed {
    ECS::Entity member;
    SpatialBounds bounds;
};

// What testing every member would return, in no particular order.
std::vector<std::uint64_t> BruteForce(const std::vector<Placed> &placed,
                                      const SpatialQuery &query) {
    std::vector<std::uint64_t> found;
    for (const Placed &p : placed) {
        if (query.Overlaps(p.bounds)) {
            found.push_back(p.member.Raw());
        }
    }
    std::sort(found.begin(), found.end());
    return found;
}

std::vector<std::uint64_t> Sorted(const std::vector<ECS::Entity> &found) {
    std::vector<std::uint64_t> raw;
    for (const ECS::Entity entity : found) {
        raw.push_back(entity.Raw());
    }
    std::sort(raw.begin(), raw.end());
    return raw;
}

bool NoneTwice(const std::vector<ECS::Entity> &found) {
    std::set<std::uint64_t> seen;
    for (const ECS::Entity entity : found) {
        if (!seen.insert(entity.Raw()).second) {
            return false;
        }
    }
    return true;
}

// Members spread over 2 km on the ground and from 800 m deep to 1,500 m high: points, and boxes up
// to 240 m across that sit in many cells.
std::vector<Placed> Scatter(SpatialIndex &index, std::mt19937_64 &random, int count) {
    std::uniform_real_distribution<double> ground(-1'000.0, 1'000.0);
    std::uniform_real_distribution<double> height(-800.0, 1'500.0);
    std::uniform_real_distribution<float> half(0.5f, 120.0f);
    std::vector<Placed> placed;
    for (int i = 0; i < count; ++i) {
        Placed p{Member(static_cast<std::uint64_t>(i) + 1),
                 {At(ground(random), height(random), ground(random))}};
        if (i % 5 == 0) {
            p.bounds.halfExtents = Vec3(half(random), half(random) * 0.25f, half(random));
        }
        index.Insert(p.member, p.bounds);
        placed.push_back(p);
    }
    return placed;
}

SpatialQuery RandomQuery(std::mt19937_64 &random, int shape) {
    std::uniform_real_distribution<double> ground(-1'100.0, 1'100.0);
    std::uniform_real_distribution<double> height(-900.0, 1'600.0);
    std::uniform_real_distribution<float> size(1.0f, 600.0f);
    const WorldPosition center = At(ground(random), height(random), ground(random));
    switch (shape % 3) {
    case 0:
        return SpatialQuery::Sphere(center, size(random));
    case 1:
        return SpatialQuery::Box(center, Vec3(size(random), size(random) * 0.5f, size(random)));
    default: {
        const float horizontal = size(random);
        return SpatialQuery::Ellipsoid(center, Vec3(horizontal, horizontal * 0.3f, horizontal));
    }
    }
}

} // namespace

TEST(SpatialIndex, AColumnHoldsOnlyItsOccupiedCells) {
    SpatialIndex index; // 64 m cells
    // A cave 500 m under a town and an island 2 km above it: one column, three cells.
    index.Insert(Member(1), {At(10.0, -500.0, 10.0)});
    index.Insert(Member(2), {At(12.0, 1.0, 11.0)});
    index.Insert(Member(3), {At(11.0, 2'000.0, 9.0)});
    index.Insert(Member(4), {At(13.0, 3.0, 12.0)}); // the town's cell again
    SpatialIndexStats stats = index.Stats();
    EXPECT_EQ(stats.members, 4u);
    EXPECT_EQ(stats.columns, 1u);
    EXPECT_EQ(stats.cells, 3u);
    EXPECT_EQ(stats.memberships, 4u);
    EXPECT_EQ(index.MembersIn(index.Space().CellOf(At(10.0, 0.0, 10.0))), 2u);

    index.Remove(Member(1));
    EXPECT_EQ(index.Stats().cells, 2u); // the cave's cell is gone
    index.Remove(Member(2));
    index.Remove(Member(3));
    index.Remove(Member(4));
    stats = index.Stats();
    EXPECT_EQ(stats.members, 0u);
    EXPECT_EQ(stats.cells, 0u);
    EXPECT_EQ(stats.columns, 0u);
}

TEST(SpatialIndex, QueriesMatchBruteForceIn3D) {
    std::mt19937_64 random(20'261'118);
    SpatialIndex index;
    std::vector<Placed> placed = Scatter(index, random, 5'000);
    std::vector<ECS::Entity> found;
    std::size_t total = 0;
    for (int round = 0; round < 2; ++round) {
        for (int q = 0; q < 900; ++q) {
            const SpatialQuery query = RandomQuery(random, q);
            found.clear();
            index.Query(query, found);
            ASSERT_TRUE(NoneTwice(found)) << "query " << q;
            ASSERT_EQ(Sorted(found), BruteForce(placed, query))
                << "round " << round << " query " << q;
            total += found.size();
        }
        // Then the members move: most a little, some across cells and far, some leave and come
        // back elsewhere.
        std::uniform_real_distribution<double> small(-3.0, 3.0);
        std::uniform_real_distribution<double> far(-300.0, 300.0);
        for (std::size_t i = 0; i < placed.size(); ++i) {
            Placed &p = placed[i];
            if (i % 10 == 0) {
                index.Remove(p.member);
                p.bounds.center = At(far(random), far(random), far(random));
                index.Insert(p.member, p.bounds);
            } else {
                const double step = i % 3 == 0 ? far(random) : small(random);
                p.bounds.center = {p.bounds.center.x + step, p.bounds.center.y - step * 0.5,
                                   p.bounds.center.z + step};
                index.Update(p.member, p.bounds);
            }
        }
    }
    EXPECT_GT(total, 50'000u); // the queries did find things
}

TEST(SpatialIndex, AMemberInManyCellsIsFoundOnce) {
    SpatialIndex index;
    // A 500 m ship: 9 cells long, 3 tall, 3 wide (it straddles the cell boundaries).
    const SpatialBounds ship{At(32.0, 32.0, 32.0), Vec3(250.0f, 40.0f, 60.0f)};
    index.Insert(Member(7), ship);
    const CellRange cells = index.CellsOf(Member(7));
    EXPECT_EQ(cells, CellsTouched(index.Space(), ship));
    EXPECT_EQ(index.Stats().memberships, cells.Count());
    EXPECT_GT(cells.Count(), 27u);

    std::vector<ECS::Entity> found;
    index.Query(SpatialQuery::Sphere(At(32.0, 32.0, 32.0), 400.0f), found);
    ASSERT_EQ(found.size(), 1u);
    EXPECT_EQ(found[0], Member(7));
    found.clear();
    index.Query(SpatialQuery::Box(At(250.0, 60.0, 80.0), Vec3(5.0f)), found); // near one end
    EXPECT_EQ(found.size(), 1u);
}

TEST(SpatialIndex, AMoveInsideItsCellsChangesNoCell) {
    SpatialIndex index;
    index.Insert(Member(1), {At(10.0, 10.0, 10.0)});
    EXPECT_FALSE(index.Update(Member(1), {At(50.0, 12.0, 30.0)})); // same cell
    EXPECT_EQ(index.Stats().cellChanges, 0u);
    EXPECT_EQ(index.BoundsOf(Member(1)).center, At(50.0, 12.0, 30.0));
    EXPECT_TRUE(index.Update(Member(1), {At(70.0, 12.0, 30.0)}));  // into the next cell along x
    EXPECT_EQ(index.Stats().cellChanges, 2u);                      // left one, entered one
    EXPECT_TRUE(index.Update(Member(1), {At(70.0, -70.0, 30.0)})); // down two cells
    std::vector<ECS::Entity> found;
    index.Query(SpatialQuery::Sphere(At(70.0, -70.0, 30.0), 1.0f), found);
    EXPECT_EQ(found.size(), 1u);
    EXPECT_EQ(index.Stats().cells, 1u);
}

TEST(SpatialIndex, ResultsComeInCellOrder) {
    SpatialIndex index;
    // Inserted out of order: z first, then x, then y.
    index.Insert(Member(5), {At(130.0, 10.0, 10.0)}); // cell x+2
    index.Insert(Member(4), {At(10.0, 10.0, 70.0)});  // cell z+1
    index.Insert(Member(3), {At(10.0, 70.0, 10.0)});  // cell y+1
    index.Insert(Member(2), {At(10.0, 10.0, 10.0)});  // cell (0,0,0)
    index.Insert(Member(1), {At(12.0, 11.0, 10.0)});  // the same cell, entered last
    std::vector<ECS::Entity> found;
    index.Query(SpatialQuery::Box(At(70.0, 40.0, 40.0), Vec3(200.0f)), found);
    const std::vector<ECS::Entity> expected{Member(2), Member(1), Member(3), Member(4), Member(5)};
    EXPECT_EQ(found, expected);
}

TEST(SpatialIndex, EveryQueryReportsInCellOrderHoweverWideItIs) {
    // A query whose box covers more columns than the index has walks the index's columns instead
    // of its range; either way the members come in the documented order.
    std::mt19937_64 random(7);
    SpatialIndex index;
    const std::vector<Placed> placed = Scatter(index, random, 150);
    const SpatialQuery queries[] = {
        SpatialQuery::Box(At(0.0, 0.0, 0.0), Vec3(100.0f)),   // a few columns: the range
        SpatialQuery::Box(At(0.0, 0.0, 0.0), Vec3(1'150.0f)), // about as many as the index
        SpatialQuery::Sphere(kFarAway, 3'000'000.0f),         // billions: the index
    };
    std::vector<ECS::Entity> found;
    for (const SpatialQuery &query : queries) {
        found.clear();
        index.Query(query, found);
        EXPECT_EQ(Sorted(found), BruteForce(placed, query));
        // The documented order: by the first of the member's cells inside the query's range (x,
        // then z, then y), then by when it entered (here, the order of insertion).
        const CellRange range = CellsTouched(index.Space(), query.Bounds());
        std::vector<ECS::Entity> expected = found;
        const auto key = [&](ECS::Entity member) {
            const CellRange cells = index.CellsOf(member);
            return std::tuple(std::max(cells.min.x, range.min.x),
                              std::max(cells.min.z, range.min.z),
                              std::max(cells.min.y, range.min.y), member.Raw());
        };
        std::sort(expected.begin(), expected.end(),
                  [&](ECS::Entity a, ECS::Entity b) { return key(a) < key(b); });
        EXPECT_EQ(found, expected);
    }
    EXPECT_EQ(found.size(), placed.size()); // the widest found every member
}

TEST(SpatialIndex, ManyThreadsQueryAtOnce) {
    // A query only reads: four threads querying the same index together get what one gets alone.
    std::mt19937_64 random(3);
    SpatialIndex index;
    const std::vector<Placed> placed = Scatter(index, random, 2'000);
    std::vector<SpatialQuery> queries;
    for (int q = 0; q < 200; ++q) {
        queries.push_back(RandomQuery(random, q));
    }
    std::vector<std::vector<ECS::Entity>> alone(queries.size());
    for (std::size_t q = 0; q < queries.size(); ++q) {
        index.Query(queries[q], alone[q]);
    }
    constexpr int kThreads = 4;
    std::vector<std::vector<std::vector<ECS::Entity>>> together(
        kThreads, std::vector<std::vector<ECS::Entity>>(queries.size()));
    std::vector<std::thread> threads;
    for (int t = 0; t < kThreads; ++t) {
        threads.emplace_back([&, t] {
            for (std::size_t q = 0; q < queries.size(); ++q) {
                index.Query(queries[q], together[t][q]);
            }
        });
    }
    for (std::thread &thread : threads) {
        thread.join();
    }
    for (int t = 0; t < kThreads; ++t) {
        EXPECT_EQ(together[t], alone) << "thread " << t;
    }
}

TEST(SpatialQuery, TheTestsAreExactAtTheBoundary) {
    const SpatialBounds box{At(10.0, 0.0, 0.0), Vec3(1.0f)};
    // The box's nearest face is 9 m from the origin along x.
    EXPECT_TRUE(SpatialQuery::Sphere(At(0.0, 0.0, 0.0), 9.0f).Overlaps(box));
    EXPECT_FALSE(SpatialQuery::Sphere(At(0.0, 0.0, 0.0), 8.999f).Overlaps(box));
    // Its corner (9, 1, 1) is sqrt(83) m away diagonally.
    const SpatialBounds corner{At(10.0, 2.0, 2.0), Vec3(1.0f)};
    EXPECT_TRUE(SpatialQuery::Sphere(At(0.0, 0.0, 0.0), 9.111f).Overlaps(corner));
    EXPECT_FALSE(SpatialQuery::Sphere(At(0.0, 0.0, 0.0), 9.110f).Overlaps(corner));
    // Boxes touching at a face overlap.
    EXPECT_TRUE(SpatialQuery::Box(At(0.0, 0.0, 0.0), Vec3(9.0f)).Overlaps(box));
    EXPECT_FALSE(SpatialQuery::Box(At(0.0, 0.0, 0.0), Vec3(8.99f, 9.0f, 9.0f)).Overlaps(box));
    // An ellipsoid 100 m wide and 10 m tall reaches a point 99 m away on the ground, not one 11 m
    // above.
    const SpatialQuery flat =
        SpatialQuery::Ellipsoid(At(0.0, 0.0, 0.0), Vec3(100.0f, 10.0f, 100.0f));
    EXPECT_TRUE(flat.Contains(At(99.0, 0.0, 0.0)));
    EXPECT_TRUE(flat.Contains(At(0.0, 0.0, -100.0)));
    EXPECT_FALSE(flat.Contains(At(0.0, 11.0, 0.0)));
    EXPECT_FALSE(flat.Contains(At(71.0, 0.0, 71.0)));
    EXPECT_TRUE(flat.Contains(At(70.0, 0.0, 70.0)));
    // The ellipsoid reaches a box whose nearest point is inside it.
    EXPECT_TRUE(flat.Overlaps({At(0.0, 15.0, 0.0), Vec3(5.0f)}));
    EXPECT_FALSE(flat.Overlaps({At(0.0, 16.0, 0.0), Vec3(5.0f)}));
}

TEST(SpatialIndex, CellsAndMembersAreListedForTools) {
    SpatialIndex index;
    index.Insert(Member(1), {At(10.0, 10.0, 10.0)});
    index.Insert(Member(2), {At(10.0, 10.0, 70.0)});
    index.Insert(Member(3), {At(11.0, 10.0, 11.0)});
    std::vector<SpatialCellInfo> cells;
    const CellCoord first = index.Space().CellOf(At(10.0, 10.0, 10.0));
    index.Cells({first, {first.x, first.y, first.z + 1}}, cells);
    ASSERT_EQ(cells.size(), 2u);
    EXPECT_EQ(cells[0].cell, first);
    EXPECT_EQ(cells[0].members, 2u);
    EXPECT_EQ(cells[1].members, 1u);
    std::vector<ECS::Entity> members;
    index.MembersOf(first, members);
    EXPECT_EQ(members, (std::vector<ECS::Entity>{Member(1), Member(3)}));
}

#if RUNIC_ASSERTS_ENABLED

TEST(SpatialIndexDeathTest, AMemberIsInTheIndexOnce) {
    SpatialIndex index;
    index.Insert(Member(1), {At(0.0, 0.0, 0.0)});
    EXPECT_DEATH(index.Insert(Member(1), {At(1.0, 0.0, 0.0)}), "already a member");
    EXPECT_DEATH(index.Update(Member(2), {At(1.0, 0.0, 0.0)}), "not a member");
    EXPECT_DEATH(index.Remove(Member(2)), "not a member");
    EXPECT_DEATH(index.Insert(ECS::Entity(), {At(1.0, 0.0, 0.0)}), "the null entity");
    EXPECT_DEATH(SpatialQuery::Sphere(kFarAway, 0.0f), "above zero");
}

#endif
