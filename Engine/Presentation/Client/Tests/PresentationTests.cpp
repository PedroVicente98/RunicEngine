#include "ClientCamera.hpp"
#include "PresentationLayer.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <memory>

#include "CommandQueue.hpp"
#include "DebugShapes.hpp"
#include "EntityWorld.hpp"
#include "RenderData/RenderFrameExchange.hpp"
#include "SimulationRuntime.hpp"
#include "WorldComponents.hpp"

using namespace Runic;
using namespace Runic::Client;
using Core::RenderInstanceId;
using Core::Vec3;
using Core::WorldPosition;

namespace {

const RenderData::RenderFrameCapacity kSmallFrame{{}, 64};
const Vec3 kLookForward(0.0f, 0.0f, -1.0f); // a Godot camera looks down its -Z

Vec3 ForwardOf(const RenderData::Camera &camera) { return camera.rotation * kLookForward; }

void ExpectNear(Vec3 actual, Vec3 expected, float tolerance) {
    EXPECT_NEAR(actual.x, expected.x, tolerance);
    EXPECT_NEAR(actual.y, expected.y, tolerance);
    EXPECT_NEAR(actual.z, expected.z, tolerance);
}

// Draws a line from the origin each frame, to show what the layer clears.
class OneLine final : public Extractor {
  public:
    void Extract(const ExtractionView &, RenderData::RenderFrame &frame) override {
        frame.debug.AddLine(RenderData::DebugBatch::WorldDepth, Vec3(0.0f), Vec3(1.0f),
                            RenderData::kDebugText);
    }
};

Application::FrameTime FrameNumber(std::uint64_t frame) {
    Application::FrameTime time;
    time.frame = frame;
    return time;
}

} // namespace

TEST(ClientCamera, ANewCameraLooksAtTheWorldsOrigin) {
    const ClientCamera camera;
    EXPECT_EQ(camera.Eye(), kDefaultEye);
    EXPECT_EQ(camera.Target(), kDefaultTarget);
    const RenderData::Camera view = camera.ViewFrom(camera.Eye());
    EXPECT_EQ(view.position, Vec3(0.0f));
    ExpectNear(ForwardOf(view), glm::normalize(Vec3(0.0f, -5.0f, -10.0f)), 1e-6f);
    EXPECT_EQ(view.farPlaneM, RenderData::kDefaultFarPlaneM);
}

TEST(ClientCamera, LooksAtItsTargetAsExactlyFarFromTheWorldsOrigin) {
    ClientCamera camera;
    const WorldPosition eye{7'000'000.25, 30.0, -9'000'000.75};
    camera.LookAt(eye, {eye.x + 10.0, eye.y, eye.z});
    const RenderData::Camera view = camera.ViewFrom(eye);
    ExpectNear(ForwardOf(view), Vec3(1.0f, 0.0f, 0.0f), 1e-6f);
    // Seen from another origin, the camera sits where the eye is relative to it.
    const RenderData::Camera shifted = camera.ViewFrom({eye.x - 3.0, eye.y, eye.z});
    EXPECT_EQ(shifted.position, Vec3(3.0f, 0.0f, 0.0f));
}

TEST(ClientCamera, LookingStraightDownStillGivesAWholeRotation) {
    ClientCamera camera;
    camera.LookAt({0.0, 50.0, 0.0}, {0.0, 0.0, 0.0});
    const RenderData::Camera view = camera.ViewFrom(camera.Eye());
    ExpectNear(ForwardOf(view), Vec3(0.0f, -1.0f, 0.0f), 1e-6f);
    EXPECT_FALSE(std::isnan(view.rotation.w));
    EXPECT_NEAR(glm::length(view.rotation), 1.0f, 1e-6f);
}

TEST(ClientCamera, FollowsAnEntityAndStopsWhenItIsGone) {
    ECS::EntityWorld world;
    World::RegisterWorldComponents(world);
    const ECS::Entity hero = world.Create();
    world.Set(hero, World::WorldTransform{{100.0, 0.0, 200.0}, World::kNoRotation});
    ClientCamera camera;
    camera.Follow(hero, Vec3(0.0f, 3.0f, 6.0f), Vec3(0.0f, 1.0f, 0.0f));
    camera.Update(world);
    EXPECT_EQ(camera.Eye(), (WorldPosition{100.0, 3.0, 206.0}));
    EXPECT_EQ(camera.Target(), (WorldPosition{100.0, 1.0, 200.0}));

    world.Get<World::WorldTransform>(hero).position.x = 140.0;
    camera.Update(world);
    EXPECT_EQ(camera.Eye().x, 140.0);

    world.Destroy(hero);
    camera.Update(world);
    EXPECT_FALSE(camera.IsFollowing());
    EXPECT_EQ(camera.Eye().x, 140.0); // where it was
}

TEST(PresentationLayer, PublishesOneFramePerUpdateSeenFromTheCamera) {
    RenderData::RenderFrameExchange frames(kSmallFrame);
    PresentationLayer layer({&frames, nullptr, nullptr});
    const WorldPosition eye{1'000'000.0, 20.0, 0.0};
    layer.Camera().LookAt(eye, {1'000'000.0, 0.0, -20.0});
    layer.OnUpdate(FrameNumber(0));
    layer.OnUpdate(FrameNumber(1));

    EXPECT_EQ(layer.FramesWritten(), 2u);
    const RenderData::RenderFrame *frame = frames.ReadNewest();
    ASSERT_NE(frame, nullptr);
    EXPECT_EQ(frame->number, 2u);
    EXPECT_EQ(frame->origin, eye); // the frame is the camera's
    EXPECT_EQ(frame->camera.position, Vec3(0.0f));
    ExpectNear(ForwardOf(frame->camera), glm::normalize(Vec3(0.0f, -20.0f, -20.0f)), 1e-6f);
}

TEST(PresentationLayer, EmptiesTheDebugGeometryOfEveryFrame) {
    RenderData::RenderFrameExchange frames(kSmallFrame);
    PresentationLayer layer({&frames, nullptr, nullptr});
    layer.Extractors().Emplace<OneLine>("engine:test_line");
    constexpr int kFrames = 5; // more than the exchange's three slots
    for (int i = 0; i < kFrames; ++i) {
        layer.OnUpdate(FrameNumber(static_cast<std::uint64_t>(i)));
    }
    const RenderData::RenderFrame *frame = frames.ReadNewest();
    EXPECT_EQ(frame->debug.VertexCount(RenderData::DebugBatch::WorldDepth,
                                       RenderData::DebugPrimitive::Lines),
              RenderData::kVerticesPerLine);
}

TEST(PresentationLayer, TheFrameAloneDescribesWhatIsDrawn) {
    // The world can be gone once the frame is written: the frame holds what the presenter needs.
    RenderData::RenderFrameExchange frames(kSmallFrame);
    auto world = std::make_unique<ECS::EntityWorld>();
    PresentationLayer layer({&frames, world.get(), nullptr});
    layer.Extractors().Emplace<DebugShapeExtractor>(std::string(kDebugShapeExtractorName), *world,
                                                    kSmallFrame.shapeInstances);
    const ECS::Entity entity = world->Create();
    world->Set(entity, World::WorldTransform{{3.0, 0.0, -4.0}, World::kNoRotation});
    world->Set(entity, DebugShape{RenderData::ShapeKind::Box, Vec3(2.0f), Vec3(0.0f),
                                  RenderData::kDebugHarmful});
    world->Set(entity, RenderIdentity{RenderInstanceId(42)});
    layer.Camera().LookAt({0.0, 0.0, 0.0}, {0.0, 0.0, -1.0});
    layer.OnUpdate(FrameNumber(0));
    world.reset();

    const RenderData::RenderFrame *frame = frames.ReadNewest();
    ASSERT_EQ(frame->shapes.Instances().size(), 1u);
    const RenderData::ShapeInstance &shape = frame->shapes.Instances()[0];
    EXPECT_EQ(shape.id, RenderInstanceId(42));
    EXPECT_EQ(shape.position, Vec3(3.0f, 0.0f, -4.0f));
    EXPECT_EQ(shape.size, Vec3(2.0f));
    EXPECT_EQ(shape.color, RenderData::kDebugHarmful);
}

TEST(PresentationLayer, TheCameraFollowsAnEntityOfTheClientWorld) {
    RenderData::RenderFrameExchange frames(kSmallFrame);
    ECS::EntityWorld world;
    World::RegisterWorldComponents(world);
    const ECS::Entity hero = world.Create();
    world.Set(hero, World::WorldTransform{{500.0, 0.0, 0.0}, World::kNoRotation});
    PresentationLayer layer({&frames, &world, nullptr});
    layer.Camera().Follow(hero, Vec3(0.0f, 2.0f, 4.0f), Vec3(0.0f));
    layer.OnUpdate(FrameNumber(0));
    EXPECT_EQ(frames.ReadNewest()->origin, (WorldPosition{500.0, 2.0, 4.0}));
}

namespace {

struct Steer {
    ECS::Entity entity;
    Vec3 velocity{0.0f}; // metres per second on the plane
};

struct Velocity {
    Vec3 value{0.0f};
};

} // namespace

TEST(PresentationLayer, EntitiesMovedByCommandsAreDrawnWhereTheSimulationPutThem) {
    // Slice 5's result in one process: commands steer entities on a plane in the simulation's
    // Commands phase, Movement moves them, and the frames show them where they are.
    Simulation::SimulationRuntime runtime;
    ECS::EntityWorld &world = runtime.Entities();
    RegisterClientComponents(world);
    world.RegisterComponent<Velocity>("engine:test_velocity");
    Simulation::CommandQueue<Steer> steering(runtime, "engine:test_steer");
    const auto moving = world.MakeQuery<World::WorldTransform, const Velocity>("engine:test_move");
    runtime.AddSystem({"engine:test_steering", Simulation::ScheduleSlot::Hz30Realtime,
                       [&](const Simulation::TickContext &) {
                           steering.Drain([&](const Steer &steer) {
                               world.Get<Velocity>(steer.entity).value = steer.velocity;
                           });
                       },
                       Simulation::RealtimePhase::Commands});
    runtime.AddSystem({"engine:test_movement", Simulation::ScheduleSlot::Hz30Realtime,
                       [&](const Simulation::TickContext &context) {
                           moving.Each([&](ECS::Entity, World::WorldTransform &transform,
                                           const Velocity &velocity) {
                               transform.position =
                                   transform.position + velocity.value * context.dt;
                           });
                       },
                       Simulation::RealtimePhase::Movement});

    const ECS::Entity walker = world.Create();
    world.Set(walker, World::WorldTransform{});
    world.Set(walker, Velocity{});
    world.Set(walker, DebugShape{});
    world.Set(walker, RenderIdentity{RenderInstanceId(1)});

    RenderData::RenderFrameExchange frames(kSmallFrame);
    PresentationLayer layer({&frames, &world, nullptr});
    layer.Extractors().Emplace<DebugShapeExtractor>(std::string(kDebugShapeExtractorName), world,
                                                    kSmallFrame.shapeInstances);
    layer.Camera().LookAt({0.0, 10.0, 10.0}, {0.0, 0.0, 0.0});

    steering.Push({walker, Vec3(3.0f, 0.0f, 0.0f)}); // 3 m/s along +X
    constexpr int kOneSecond = 60;
    for (int tick = 0; tick < kOneSecond; ++tick) {
        runtime.RunMasterTick(); // the Simulation layer's work
        layer.OnUpdate(FrameNumber(static_cast<std::uint64_t>(tick)));
    }
    const RenderData::RenderFrame *frame = frames.ReadNewest();
    ASSERT_EQ(frame->shapes.Instances().size(), 1u);
    // 30 realtime ticks of 1/30 s at 3 m/s, seen from the camera at (0, 10, 10).
    const Vec3 drawn = frame->shapes.Instances()[0].position;
    ExpectNear(drawn, Vec3(3.0f, -10.0f, -10.0f), 1e-4f);
}

#if RUNIC_ASSERTS_ENABLED

TEST(ClientCameraDeathTest, ACameraCannotLookAtItsOwnEye) {
    ClientCamera camera;
    EXPECT_DEATH(camera.LookAt({1.0, 2.0, 3.0}, {1.0, 2.0, 3.0}), "look at its own eye");
}

TEST(PresentationLayerDeathTest, TheLayerNeedsAFrameExchange) {
    EXPECT_DEATH(PresentationLayer({nullptr, nullptr, nullptr}), "needs a frame exchange");
}

#endif // RUNIC_ASSERTS_ENABLED
