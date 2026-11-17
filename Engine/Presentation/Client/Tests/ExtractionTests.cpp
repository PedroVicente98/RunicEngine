#include "DebugShapes.hpp"
#include "ExtractorRegistry.hpp"

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

#include "EntityWorld.hpp"
#include "MetricsRegistry.hpp"
#include "RenderData/RenderFrame.hpp"
#include "WorldComponents.hpp"

using namespace Runic;
using namespace Runic::Client;
using Core::RenderInstanceId;
using Core::Vec3;
using Core::WorldPosition;
using RenderData::RenderFrame;
using RenderData::ShapeKind;

namespace {

// Records that it ran, in a shared list.
class Recorder final : public Extractor {
  public:
    Recorder(std::vector<std::string> &ran, std::string name)
        : m_Ran(ran), m_Name(std::move(name)) {}
    void Extract(const ExtractionView &, RenderFrame &) override { m_Ran.push_back(m_Name); }

  private:
    std::vector<std::string> &m_Ran;
    std::string m_Name;
};

const RenderData::RenderFrameCapacity kSmallFrame{{}, 64};

ExtractionView ViewFrom(WorldPosition camera, float drawDistanceM = 100.0f) {
    ExtractionView view;
    view.origin = camera;
    view.camera = camera;
    view.drawDistanceM = drawDistanceM;
    return view;
}

ECS::Entity MakeDrawable(ECS::EntityWorld &world, std::uint64_t id, WorldPosition position,
                         const DebugShape &shape = {}) {
    const ECS::Entity entity = world.Create();
    world.Set(entity, World::WorldTransform{position, World::kNoRotation});
    world.Set(entity, shape);
    world.Set(entity, RenderIdentity{RenderInstanceId(id)});
    return entity;
}

} // namespace

TEST(ExtractorRegistry, RunsEachExtractorOnceInTheOrderTheyWereRegistered) {
    ExtractorRegistry registry;
    std::vector<std::string> ran;
    registry.Emplace<Recorder>("engine:test_second", ran, "second");
    registry.Emplace<Recorder>("game:test_first", ran, "first");
    RenderFrame frame(kSmallFrame);
    registry.Run({}, frame);
    registry.Run({}, frame);
    EXPECT_EQ(ran, (std::vector<std::string>{"second", "first", "second", "first"}));
}

TEST(ExtractorRegistry, FindsAndDescribesExtractorsByName) {
    ExtractorRegistry registry;
    std::vector<std::string> ran;
    Extractor &added = registry.Emplace<Recorder>("engine:test_shapes", ran, "shapes");
    ASSERT_EQ(registry.Count(), 1u);
    EXPECT_EQ(registry.Describe(0).name, "engine:test_shapes");
    EXPECT_EQ(registry.Find("engine:test_shapes"), &added);
    EXPECT_EQ(registry.Find("engine:test_missing"), nullptr);
}

TEST(ExtractorRegistry, RecordsTheTimeOfEachExtractorAndTheFrames) {
    Core::MetricsRegistry metrics;
    ExtractorRegistry registry(&metrics);
    std::vector<std::string> ran;
    registry.Emplace<Recorder>("engine:test_shapes", ran, "shapes");
    RenderFrame frame(kSmallFrame);
    constexpr int kFrames = 3;
    for (int i = 0; i < kFrames; ++i) {
        registry.Run({}, frame);
    }
    const Core::TimingId time = metrics.AddTiming(
        {std::string(kExtractTimeMetricPrefix) + "engine:test_shapes", Core::kNoMetricDomain, ""});
    EXPECT_EQ(metrics.Read(time).count, static_cast<std::uint64_t>(kFrames));
    const Core::CounterId frames =
        metrics.AddCounter({kFramesExtractedMetric, Core::kNoMetricDomain, ""});
    EXPECT_EQ(metrics.Read(frames), static_cast<std::uint64_t>(kFrames));
}

TEST(DebugShapeExtractor, DrawsAnEntityAtItsPlaceRelativeToTheCamera) {
    ECS::EntityWorld world;
    DebugShapeExtractor extractor(world, kSmallFrame.shapeInstances);
    DebugShape shape;
    shape.shape = ShapeKind::Capsule;
    shape.size = Vec3(0.8f, 1.8f, 0.8f);
    shape.offset = Vec3(0.0f, 0.9f, 0.0f);
    shape.color = RenderData::kDebugPredicted;
    const WorldPosition far{3'000'000.0, 12.0, -4'500'000.0}; // thousands of km from the origin
    MakeDrawable(world, 7, far, shape);

    RenderFrame frame(kSmallFrame);
    const WorldPosition camera{3'000'010.0, 17.0, -4'500'000.0};
    extractor.Extract(ViewFrom(camera), frame);

    ASSERT_EQ(frame.shapes.Instances().size(), 1u);
    const RenderData::ShapeInstance &drawn = frame.shapes.Instances()[0];
    EXPECT_EQ(drawn.id, RenderInstanceId(7));
    EXPECT_EQ(drawn.shape, ShapeKind::Capsule);
    EXPECT_EQ(drawn.position.x, -10.0f);
    EXPECT_NEAR(drawn.position.y, -4.1f, 1e-6f); // the center: 0.9 m above the feet
    EXPECT_EQ(drawn.position.z, 0.0f);
    EXPECT_EQ(drawn.size, shape.size);
    EXPECT_EQ(drawn.color, RenderData::kDebugPredicted);
    EXPECT_EQ(extractor.Stats().drawn, 1u);
}

TEST(DebugShapeExtractor, TheOffsetTurnsWithTheEntity) {
    ECS::EntityWorld world;
    DebugShapeExtractor extractor(world, kSmallFrame.shapeInstances);
    DebugShape shape;
    shape.offset = Vec3(1.0f, 0.0f, 0.0f);
    const ECS::Entity entity = MakeDrawable(world, 1, {}, shape);
    const Core::Quat quarterTurnAboutY{0.70710678f, 0.0f, 0.70710678f, 0.0f};
    world.Get<World::WorldTransform>(entity).rotation = quarterTurnAboutY;

    RenderFrame frame(kSmallFrame);
    extractor.Extract(ViewFrom({}), frame);
    const RenderData::ShapeInstance &drawn = frame.shapes.Instances()[0];
    EXPECT_NEAR(drawn.position.x, 0.0f, 1e-6f);
    EXPECT_NEAR(drawn.position.z, -1.0f, 1e-6f); // +X turned a quarter about +Y is -Z
    EXPECT_EQ(drawn.rotation, quarterTurnAboutY);
}

TEST(DebugShapeExtractor, OnlyEntitiesWithAPlaceAShapeAndAnIdentityAreDrawn) {
    ECS::EntityWorld world;
    DebugShapeExtractor extractor(world, kSmallFrame.shapeInstances);
    MakeDrawable(world, 1, {});
    const ECS::Entity noIdentity = world.Create();
    world.Set(noIdentity, World::WorldTransform{});
    world.Set(noIdentity, DebugShape{});
    const ECS::Entity noShape = world.Create();
    world.Set(noShape, World::WorldTransform{});
    world.Set(noShape, RenderIdentity{RenderInstanceId(2)});

    RenderFrame frame(kSmallFrame);
    extractor.Extract(ViewFrom({}), frame);
    ASSERT_EQ(frame.shapes.Instances().size(), 1u);
    EXPECT_EQ(frame.shapes.Instances()[0].id, RenderInstanceId(1));
}

TEST(DebugShapeExtractor, EntitiesBeyondTheDrawDistanceAreLeftOut) {
    ECS::EntityWorld world;
    DebugShapeExtractor extractor(world, kSmallFrame.shapeInstances);
    MakeDrawable(world, 1, {50.0, 0.0, 0.0});
    MakeDrawable(world, 2, {150.0, 0.0, 0.0});
    RenderFrame frame(kSmallFrame);
    extractor.Extract(ViewFrom({}, 100.0f), frame);
    EXPECT_EQ(frame.shapes.Instances().size(), 1u);
    EXPECT_EQ(extractor.Stats().beyondDrawDistance, 1u);

    extractor.Extract(ViewFrom({100.0, 0.0, 0.0}, 100.0f), frame); // the camera walked closer
    EXPECT_EQ(frame.shapes.Instances().size(), 2u);
    EXPECT_TRUE(frame.shapes.LayoutChanged());
}

TEST(DebugShapeExtractor, AStillWorldKeepsItsRevisionWhileTheCameraMoves) {
    ECS::EntityWorld world;
    DebugShapeExtractor extractor(world, kSmallFrame.shapeInstances);
    for (std::uint64_t id = 1; id <= 10; ++id) {
        MakeDrawable(world, id, {static_cast<double>(id), 0.0, 0.0});
    }
    RenderFrame frame(kSmallFrame);
    extractor.Extract(ViewFrom({0.0, 2.0, 5.0}), frame);
    const std::uint64_t revision = frame.shapes.Revision();
    for (int step = 1; step <= 5; ++step) {
        extractor.Extract(ViewFrom({static_cast<double>(step), 2.0, 5.0}), frame);
        EXPECT_EQ(frame.shapes.Revision(), revision);
        EXPECT_TRUE(frame.shapes.Changed().empty());
    }
}

TEST(DebugShapeExtractor, AnEntityThatMovesIsListedAsChanged) {
    ECS::EntityWorld world;
    DebugShapeExtractor extractor(world, kSmallFrame.shapeInstances);
    MakeDrawable(world, 1, {});
    const ECS::Entity walker = MakeDrawable(world, 2, {2.0, 0.0, 0.0});
    MakeDrawable(world, 3, {4.0, 0.0, 0.0});
    RenderFrame frame(kSmallFrame);
    extractor.Extract(ViewFrom({}), frame);
    const std::uint64_t revision = frame.shapes.Revision();

    world.Get<World::WorldTransform>(walker).position.z += 0.5;
    extractor.Extract(ViewFrom({}), frame);
    EXPECT_EQ(frame.shapes.Revision(), revision + 1);
    ASSERT_EQ(frame.shapes.Changed().size(), 1u);
    EXPECT_EQ(frame.shapes.Instances()[frame.shapes.Changed()[0]].id, RenderInstanceId(2));
}

#if RUNIC_ASSERTS_ENABLED

TEST(ExtractorRegistryDeathTest, AnExtractorsNameIsUniqueAndHasANamespace) {
    ExtractorRegistry registry;
    std::vector<std::string> ran;
    registry.Emplace<Recorder>("engine:test_shapes", ran, "shapes");
    EXPECT_DEATH(registry.Emplace<Recorder>("engine:test_shapes", ran, "again"),
                 "two extractors were registered under one name");
    EXPECT_DEATH(registry.Emplace<Recorder>("shapes", ran, "bare"), "namespace:name");
    EXPECT_DEATH(registry.Register("engine:test_nothing", nullptr), "registered as nothing");
}

#endif // RUNIC_ASSERTS_ENABLED
