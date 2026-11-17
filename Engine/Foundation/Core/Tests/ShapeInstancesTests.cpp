#include "RenderData/ShapeInstances.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

#include "AllocationCounter.hpp"
#include "RenderData/RenderFrameExchange.hpp"

using namespace Runic;
using namespace Runic::RenderData;
using Core::RenderInstanceId;
using Core::Vec3;
using Core::WorldPosition;

namespace {

constexpr Core::Quat kNoTurn{1.0f, 0.0f, 0.0f, 0.0f};
const Vec3 kUnit{1.0f};

// A shape as a test describes it, in the world.
struct Placed {
    std::uint64_t id = 0;
    WorldPosition position;
    PackedColor color = kDebugInactive;
    Vec3 size{1.0f};
    ShapeKind shape = ShapeKind::Box;
};

void Write(ShapeInstanceWriter &writer, ShapeInstanceList &list, WorldPosition origin,
           const std::vector<Placed> &shapes) {
    writer.Begin(list, origin);
    for (const Placed &placed : shapes) {
        writer.Add(RenderInstanceId(placed.id), placed.shape, placed.position, kNoTurn, placed.size,
                   placed.color);
    }
    writer.End();
}

std::vector<std::uint32_t> ChangedOf(const ShapeInstanceList &list) {
    return {list.Changed().begin(), list.Changed().end()};
}

} // namespace

TEST(ShapeInstances, AListStartsEmptyAtRevisionZero) {
    const ShapeInstanceList list;
    EXPECT_TRUE(list.Instances().empty());
    EXPECT_EQ(list.Revision(), 0u);
    EXPECT_FALSE(list.LayoutChanged());
    EXPECT_EQ(list.Capacity(), kDefaultShapeInstanceCapacity);
}

TEST(ShapeInstances, TheFirstListIsANewLayoutWithPositionsRelativeToItsOrigin) {
    ShapeInstanceWriter writer;
    ShapeInstanceList list;
    const WorldPosition origin{1'000'000.0, 10.0, -2'000'000.0};
    Write(writer, list, origin,
          {{1, {1'000'003.0, 10.0, -2'000'000.0}}, {2, {999'990.5, 12.0, -2'000'004.0}}});

    EXPECT_EQ(list.Revision(), 1u);
    EXPECT_EQ(list.BaseRevision(), 0u);
    EXPECT_TRUE(list.LayoutChanged());
    ASSERT_EQ(list.Instances().size(), 2u);
    EXPECT_EQ(list.Instances()[0].id, RenderInstanceId(1));
    EXPECT_EQ(list.Instances()[0].position, Vec3(3.0f, 0.0f, 0.0f));
    EXPECT_EQ(list.Instances()[1].position, Vec3(-9.5f, 2.0f, -4.0f));
    EXPECT_EQ(writer.Revision(), 1u);
}

TEST(ShapeInstances, UnchangedShapesKeepTheRevisionEvenWhenTheOriginMoves) {
    ShapeInstanceWriter writer;
    ShapeInstanceList first;
    ShapeInstanceList second;
    const std::vector<Placed> still{{1, {5.0, 0.0, 0.0}}, {2, {0.0, 0.0, 7.0}}};
    Write(writer, first, {0.0, 0.0, 0.0}, still);
    Write(writer, second, {100.0, 0.0, 0.0}, still); // the camera walked 100 m

    EXPECT_EQ(second.Revision(), first.Revision()); // nothing for the presenter to do
    EXPECT_EQ(second.BaseRevision(), first.Revision());
    EXPECT_FALSE(second.LayoutChanged());
    EXPECT_TRUE(second.Changed().empty());
    // The numbers in the frame follow the origin all the same.
    EXPECT_EQ(second.Instances()[0].position, Vec3(-95.0f, 0.0f, 0.0f));
}

TEST(ShapeInstances, AShapeThatMovesOrChangesIsListedByItsIndex) {
    ShapeInstanceWriter writer;
    ShapeInstanceList list;
    std::vector<Placed> shapes{
        {1, {0.0, 0.0, 0.0}}, {2, {3.0, 0.0, 0.0}}, {3, {6.0, 0.0, 0.0}}, {4, {9.0, 0.0, 0.0}}};
    Write(writer, list, {}, shapes);
    const std::uint64_t base = list.Revision();

    shapes[1].position.z = 0.25;     // moved
    shapes[3].color = kDebugHarmful; // recolored
    Write(writer, list, {}, shapes);
    EXPECT_EQ(list.Revision(), base + 1);
    EXPECT_EQ(list.BaseRevision(), base);
    EXPECT_FALSE(list.LayoutChanged());
    EXPECT_EQ(ChangedOf(list), (std::vector<std::uint32_t>{1, 3}));

    shapes[0].size = Vec3(2.0f);          // resized
    shapes[2].shape = ShapeKind::Capsule; // another kind
    Write(writer, list, {}, shapes);
    EXPECT_EQ(ChangedOf(list), (std::vector<std::uint32_t>{0, 2}));
}

TEST(ShapeInstances, ARotationChangeIsAChange) {
    ShapeInstanceWriter writer;
    ShapeInstanceList list;
    writer.Begin(list, {});
    writer.Add(RenderInstanceId(1), ShapeKind::Box, {}, kNoTurn, kUnit, kDebugInactive);
    writer.End();
    const Core::Quat turned{0.0f, 0.0f, 1.0f, 0.0f}; // half a turn about y
    writer.Begin(list, {});
    writer.Add(RenderInstanceId(1), ShapeKind::Box, {}, turned, kUnit, kDebugInactive);
    writer.End();
    EXPECT_EQ(ChangedOf(list), (std::vector<std::uint32_t>{0}));
    EXPECT_EQ(list.Instances()[0].rotation, turned);
}

TEST(ShapeInstances, AddingRemovingOrReorderingShapesChangesTheLayout) {
    ShapeInstanceWriter writer;
    ShapeInstanceList list;
    Write(writer, list, {}, {{1, {}}, {2, {}}});
    const std::uint64_t start = list.Revision();

    Write(writer, list, {}, {{1, {}}, {2, {}}, {3, {}}}); // one more at the end
    EXPECT_TRUE(list.LayoutChanged());
    EXPECT_TRUE(list.Changed().empty());         // indices mean nothing across a new layout
    Write(writer, list, {}, {{1, {}}, {2, {}}}); // one fewer
    EXPECT_TRUE(list.LayoutChanged());
    Write(writer, list, {}, {{2, {}}, {1, {}}}); // the same shapes, in another order
    EXPECT_TRUE(list.LayoutChanged());
    EXPECT_EQ(list.Revision(), start + 3);
    Write(writer, list, {}, {{2, {}}, {1, {}}});
    EXPECT_FALSE(list.LayoutChanged());
    EXPECT_EQ(list.Revision(), start + 3);
}

TEST(ShapeInstances, ShapesPastTheCapacityOrOutOfReachAreDroppedAndCounted) {
    constexpr std::uint32_t kCapacity = 2;
    ShapeInstanceWriter writer(kCapacity);
    ShapeInstanceList list(kCapacity);
    writer.Begin(list, {});
    EXPECT_TRUE(writer.Add(RenderInstanceId(1), ShapeKind::Box, {}, kNoTurn, kUnit, kDebugText));
    const WorldPosition tooFar{Core::kMaxLocalOffsetM + 1.0, 0.0, 0.0};
    EXPECT_FALSE(
        writer.Add(RenderInstanceId(2), ShapeKind::Box, tooFar, kNoTurn, kUnit, kDebugText));
    EXPECT_TRUE(writer.Add(RenderInstanceId(3), ShapeKind::Box, {}, kNoTurn, kUnit, kDebugText));
    EXPECT_FALSE(writer.Add(RenderInstanceId(4), ShapeKind::Box, {}, kNoTurn, kUnit, kDebugText));
    writer.End();
    EXPECT_EQ(list.Instances().size(), kCapacity);
    EXPECT_EQ(list.Dropped(), 2u);
    EXPECT_EQ(list.Instances()[1].id, RenderInstanceId(3)); // what was dropped left no gap
}

TEST(ShapeInstances, AReaderThatSkipsAFrameSeesABaseItDidNotApply) {
    // Through the exchange: the writer publishes three frames while the reader looks once.
    RenderFrameExchange frames(RenderFrameCapacity{{}, 16});
    ShapeInstanceWriter writer(16);
    std::vector<Placed> shapes{{1, {}}, {2, {}}};

    Write(writer, frames.WriteFrame().shapes, {}, shapes);
    frames.Publish();
    const RenderFrame *frame = frames.ReadNewest();
    ASSERT_NE(frame, nullptr);
    const std::uint64_t applied = frame->shapes.Revision();

    shapes[0].position.x = 1.0;
    Write(writer, frames.WriteFrame().shapes, {}, shapes);
    frames.Publish();
    shapes[1].position.x = 1.0;
    Write(writer, frames.WriteFrame().shapes, {}, shapes);
    frames.Publish();

    frame = frames.ReadNewest(); // the newest: the reader skipped one
    EXPECT_EQ(frame->shapes.Revision(), applied + 2);
    EXPECT_NE(frame->shapes.BaseRevision(), applied); // so its Changed() is not enough
    EXPECT_EQ(ChangedOf(frame->shapes), (std::vector<std::uint32_t>{1}));
}

TEST(ShapeInstances, WritingAllocatesNothingOnceMade) {
    constexpr std::uint32_t kShapes = 1000;
    ShapeInstanceWriter writer(kShapes);
    ShapeInstanceList list(kShapes);
    std::uint64_t allocations = 0;
    {
        Core::Testing::AllocationScope scope;
        for (int frame = 0; frame < 3; ++frame) {
            writer.Begin(list, {static_cast<double>(frame), 0.0, 0.0});
            for (std::uint32_t shape = 1; shape <= kShapes; ++shape) {
                writer.Add(RenderInstanceId(shape), ShapeKind::Sphere,
                           {static_cast<double>(shape), 0.0, static_cast<double>(frame)}, kNoTurn,
                           kUnit, kDebugQuery);
            }
            writer.End();
        }
        allocations = scope.Count();
    }
    EXPECT_EQ(allocations, 0u);
    EXPECT_EQ(list.Changed().size(), kShapes); // every shape moved in the last frame
}

TEST(ShapeInstances, AFrameHoldsShapesBesideItsDebugGeometry) {
    const RenderFrame frame(RenderFrameCapacity{{}, 8});
    EXPECT_EQ(frame.shapes.Capacity(), 8u);
    const RenderFrame defaults;
    EXPECT_EQ(defaults.shapes.Capacity(), kDefaultShapeInstanceCapacity);
}

#if RUNIC_ASSERTS_ENABLED

TEST(ShapeInstancesDeathTest, AShapeNeedsAValidId) {
    ShapeInstanceWriter writer(1);
    ShapeInstanceList list(1);
    writer.Begin(list, {});
    EXPECT_DEATH(writer.Add(RenderInstanceId(), ShapeKind::Box, {}, kNoTurn, kUnit, kDebugInactive),
                 "needs a valid RenderInstanceId");
}

TEST(ShapeInstancesDeathTest, ShapesAreAddedBetweenBeginAndEnd) {
    ShapeInstanceWriter writer(1);
    EXPECT_DEATH(
        writer.Add(RenderInstanceId(1), ShapeKind::Box, {}, kNoTurn, kUnit, kDebugInactive),
        "outside Begin and End");
}

#endif // RUNIC_ASSERTS_ENABLED
