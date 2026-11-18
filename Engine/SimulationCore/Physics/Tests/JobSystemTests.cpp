// PHYS-4: Jolt's job system on the Core worker pool. A step on the pool gives the same bodies,
// characters and contact facts, to the bit, as a step on the stepping thread alone, and Jolt never
// creates a thread of its own.

#include "PhysicsWorld.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

#include "WorkerPool.hpp"

using namespace Runic;
using namespace Runic::Physics;
using Core::Vec3;
using Core::WorldPosition;

namespace {

constexpr float kTickSeconds = 1.0f / 30.0f;
constexpr int kTwoSeconds = 60;

// The scene: a bumpy mesh floor, a pile of boxes and spheres dropped onto it so they land on each
// other, a kinematic paddle sweeping through the pile, and characters walking across it. Enough
// bodies that Jolt splits the step into many jobs, and contacts on a mesh, where a pair touches
// through several triangles at once.
constexpr int kFloorCells = 20;
constexpr float kFloorCellM = 2.0f;
constexpr float kFloorBumpM = 0.15f;
constexpr int kPileSide = 6;
constexpr int kPileLayers = 4;
constexpr double kPileSpacingM = 1.1;
constexpr double kPileFirstHeightM = 2.0;
constexpr double kPileStaggerM = 0.37;
constexpr int kCharacters = 4;
constexpr float kWalkMps = 2.0f;
constexpr float kPaddleMps = 1.5f;

// Thousands of kilometres out: the pool changes nothing about the precision rule.
const WorldPosition kOrigin{3'000'000.0, 40.0, -2'000'000.0};

struct PileScene {
    std::vector<BodyHandle> bodies; // every body, the floor first
    std::vector<CharacterHandle> characters;
    BodyHandle paddle;
};

ShapeId BumpyFloor(PhysicsWorld &world) {
    std::vector<Vec3> vertices;
    const float half = kFloorCells * kFloorCellM / 2.0f;
    for (int z = 0; z <= kFloorCells; ++z) {
        for (int x = 0; x <= kFloorCells; ++x) {
            const float bump = kFloorBumpM * std::sin(static_cast<float>(x) * 1.3f) *
                               std::cos(static_cast<float>(z) * 0.9f);
            vertices.emplace_back(static_cast<float>(x) * kFloorCellM - half, bump,
                                  static_cast<float>(z) * kFloorCellM - half);
        }
    }
    std::vector<std::uint32_t> indices;
    const auto at = [](int x, int z) {
        return static_cast<std::uint32_t>(z * (kFloorCells + 1) + x);
    };
    for (int z = 0; z < kFloorCells; ++z) {
        for (int x = 0; x < kFloorCells; ++x) {
            // Counter-clockwise seen from above (+Y).
            indices.insert(indices.end(), {at(x, z), at(x, z + 1), at(x + 1, z + 1)});
            indices.insert(indices.end(), {at(x, z), at(x + 1, z + 1), at(x + 1, z)});
        }
    }
    return world.AddShape("engine:test_bumpy_floor", MeshShape(vertices, indices)).Value();
}

PileScene BuildPile(PhysicsWorld &world) {
    PileScene scene;
    const CollisionLayer still = world.Layers().Find(kStaticLayerName);
    const CollisionLayer moving = world.Layers().Find(kMovingLayerName);
    const ShapeId box = world.AddShape("engine:test_crate", BoxShape(Vec3(0.4f))).Value();
    const ShapeId ball = world.AddShape("engine:test_ball", SphereShape(0.45f)).Value();
    const ShapeId paddle =
        world.AddShape("engine:test_paddle", BoxShape(Vec3(0.3f, 1.0f, 4.0f))).Value();

    std::vector<BodyDesc> descs;
    BodyDesc floor;
    floor.shape = BumpyFloor(world);
    floor.layer = still;
    floor.pose.position = kOrigin;
    floor.owner = 1;
    descs.push_back(floor);
    std::uint64_t owner = 2;
    for (int layer = 0; layer < kPileLayers; ++layer) {
        for (int z = 0; z < kPileSide; ++z) {
            for (int x = 0; x < kPileSide; ++x) {
                BodyDesc body;
                body.shape = (x + z + layer) % 2 == 0 ? box : ball;
                body.motion = MotionType::Dynamic;
                body.layer = moving;
                const double stagger = layer % 2 == 0 ? 0.0 : kPileStaggerM;
                body.pose.position = {kOrigin.x + (x - kPileSide / 2) * kPileSpacingM + stagger,
                                      kOrigin.y + kPileFirstHeightM + layer * kPileSpacingM,
                                      kOrigin.z + (z - kPileSide / 2) * kPileSpacingM + stagger};
                body.owner = owner++;
                descs.push_back(body);
            }
        }
    }
    BodyDesc sweeper;
    sweeper.shape = paddle;
    sweeper.motion = MotionType::Kinematic;
    sweeper.layer = moving;
    sweeper.pose.position = {kOrigin.x - 12.0, kOrigin.y + 1.0, kOrigin.z};
    sweeper.owner = owner++;
    descs.push_back(sweeper);

    scene.bodies.resize(descs.size());
    world.CreateBodies(descs, scene.bodies);
    scene.paddle = scene.bodies.back();

    for (int i = 0; i < kCharacters; ++i) {
        CharacterDesc desc;
        desc.layer = world.Layers().Find(kCharacterLayerName);
        desc.pose.position = {kOrigin.x - 10.0 + i * 1.5, kOrigin.y + 0.5,
                              kOrigin.z - 6.0 + i * 3.0};
        desc.owner = owner++;
        scene.characters.push_back(world.CreateCharacter(desc).Value());
    }
    return scene;
}

// Appends the bytes of a value: the comparison is to the bit, not within a tolerance.
template <class T> void Append(std::string &out, const T &value) {
    char bytes[sizeof(T)];
    std::memcpy(bytes, &value, sizeof(T));
    out.append(bytes, sizeof(T));
}

void AppendPose(std::string &out, const Pose &pose) {
    Append(out, pose.position.x);
    Append(out, pose.position.y);
    Append(out, pose.position.z);
    Append(out, pose.rotation.w);
    Append(out, pose.rotation.x);
    Append(out, pose.rotation.y);
    Append(out, pose.rotation.z);
}

void AppendVec(std::string &out, Vec3 v) {
    Append(out, v.x);
    Append(out, v.y);
    Append(out, v.z);
}

struct StepTrace {
    std::string state; // every body's and character's state after the step, as bytes
    std::vector<ContactFact> facts;
};

std::vector<StepTrace> RunPile(Core::WorkerPool *pool) {
    PhysicsWorldSpec spec;
    spec.workers = pool;
    PhysicsWorld world(spec);
    const PileScene scene = BuildPile(world);
    std::vector<StepTrace> trace;
    for (int tick = 0; tick < kTwoSeconds; ++tick) {
        const Pose paddle = world.BodyPose(scene.paddle);
        Pose next = paddle;
        next.position.x += kPaddleMps * kTickSeconds;
        world.MoveKinematic(scene.paddle, next, kTickSeconds);
        for (std::size_t i = 0; i < scene.characters.size(); ++i) {
            const float sign = i % 2 == 0 ? 1.0f : -1.0f;
            world.MoveCharacter(scene.characters[i], {Vec3(kWalkMps, 0.0f, sign * kWalkMps)});
        }
        world.Step(kTickSeconds);

        StepTrace step;
        for (const BodyHandle body : scene.bodies) {
            AppendPose(step.state, world.BodyPose(body));
            AppendVec(step.state, world.LinearVelocity(body));
            Append(step.state, world.IsActive(body));
        }
        for (const CharacterHandle character : scene.characters) {
            const CharacterState state = world.Character(character);
            AppendPose(step.state, state.pose);
            AppendVec(step.state, state.velocity);
            Append(step.state, state.ground);
            Append(step.state, state.groundBody.Value());
        }
        world.TakeContacts(step.facts);
        trace.push_back(std::move(step));
    }
    return trace;
}

std::string FactBytes(const ContactFact &fact) {
    std::string out;
    Append(out, fact.phase);
    Append(out, fact.a.Value());
    Append(out, fact.b.Value());
    Append(out, fact.ownerA);
    Append(out, fact.ownerB);
    Append(out, fact.point.x);
    Append(out, fact.point.y);
    Append(out, fact.point.z);
    AppendVec(out, fact.normal);
    return out;
}

// The first step where two runs differ, if any.
std::optional<int> FirstDifference(const std::vector<StepTrace> &x,
                                   const std::vector<StepTrace> &y) {
    for (std::size_t step = 0; step < std::min(x.size(), y.size()); ++step) {
        bool same = x[step].state == y[step].state && x[step].facts.size() == y[step].facts.size();
        for (std::size_t i = 0; same && i < x[step].facts.size(); ++i) {
            same = FactBytes(x[step].facts[i]) == FactBytes(y[step].facts[i]);
        }
        if (!same) {
            return static_cast<int>(step);
        }
    }
    return x.size() == y.size() ? std::nullopt : std::optional<int>(static_cast<int>(x.size()));
}

std::size_t FactsOf(const std::vector<StepTrace> &trace) {
    std::size_t facts = 0;
    for (const StepTrace &step : trace) {
        facts += step.facts.size();
    }
    return facts;
}

int ThreadsOfThisProcess() {
    std::ifstream status("/proc/self/status");
    for (std::string line; std::getline(status, line);) {
        if (line.rfind("Threads:", 0) == 0) {
            return std::stoi(line.substr(line.find(':') + 1));
        }
    }
    return -1;
}

std::uint64_t JobsRunByThePool(const Core::WorkerPool &pool) {
    std::uint64_t jobs = pool.JobsRunInline();
    for (Core::WorkerIndex worker = 0; worker < pool.WorkerCount(); ++worker) {
        jobs += pool.JobsRunBy(worker);
    }
    return jobs;
}

} // namespace

TEST(PhysicsJobSystem, AStepOnThePoolMatchesAStepOnTheSteppingThreadToTheBit) {
    const std::vector<StepTrace> alone = RunPile(nullptr);
    // The scene does what it is meant to: the pile lands and its bodies touch the floor and each
    // other, and the paddle sweeps into it.
    ASSERT_GT(FactsOf(alone), static_cast<std::size_t>(kPileSide * kPileSide));

    for (const std::uint32_t workers : {1u, 2u, 4u, 8u}) {
        Core::WorkerPool pool({.workers = workers});
        const std::vector<StepTrace> pooled = RunPile(&pool);
        const std::optional<int> differs = FirstDifference(alone, pooled);
        EXPECT_FALSE(differs.has_value())
            << "with " << workers << " workers the run differs from step " << differs.value_or(-1);
    }
}

TEST(PhysicsJobSystem, AStepsFactsComeOrderedByTheirBodies) {
    Core::WorkerPool pool({.workers = 4});
    const auto key = [](const ContactFact &fact) {
        return std::tuple(fact.a.Value(), fact.b.Value(), fact.phase);
    };
    for (const StepTrace &step : RunPile(&pool)) {
        EXPECT_TRUE(std::is_sorted(
            step.facts.begin(), step.facts.end(),
            [&](const ContactFact &x, const ContactFact &y) { return key(x) < key(y); }));
    }
}

TEST(PhysicsJobSystem, ManyRunsOnManyWorkersGiveOneResult) {
    // Thread timing differs from run to run; the result must not.
    Core::WorkerPool pool({.workers = 4});
    const std::vector<StepTrace> first = RunPile(&pool);
    for (int run = 0; run < 2; ++run) {
        const std::optional<int> differs = FirstDifference(first, RunPile(&pool));
        EXPECT_FALSE(differs.has_value())
            << "run " << run << " differs from step " << differs.value_or(-1);
    }
}

TEST(PhysicsJobSystem, JoltRunsOnThePoolAndCreatesNoThread) {
    Core::WorkerPool pool({.workers = 4});
    const int before = ThreadsOfThisProcess();
    if (before < 0) {
        GTEST_SKIP() << "no /proc/self/status here";
    }
    const std::uint64_t jobsBefore = JobsRunByThePool(pool);
    const std::vector<StepTrace> trace = RunPile(&pool);
    EXPECT_EQ(ThreadsOfThisProcess(), before);
    // Every step queues Jolt's jobs on the pool, and every one has run when Step returns.
    EXPECT_GE(JobsRunByThePool(pool), jobsBefore + trace.size());
}

TEST(PhysicsJobSystem, APoolOfOneWorkerRunsTheStepOnTheSteppingThread) {
    // One worker has no other thread to hand a job to: Jolt runs every job inside the step, as it
    // waits for them, and queues nothing on the pool.
    Core::WorkerPool pool({.workers = Core::kSingleWorker});
    const std::vector<StepTrace> trace = RunPile(&pool);
    EXPECT_EQ(trace.size(), static_cast<std::size_t>(kTwoSeconds));
    EXPECT_EQ(JobsRunByThePool(pool), 0u);
}
