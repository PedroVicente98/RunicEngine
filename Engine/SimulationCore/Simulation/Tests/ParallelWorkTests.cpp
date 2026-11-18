// SIM-3: phases on the worker pool. A system's per-entity work runs in chunks across the workers,
// effects on others go to per-worker buffers merged in chunk order, and the physics step runs on
// the same pool (PHYS-4): the same scene gives the same result, to the bit, with one worker and
// with many.

#include "ParallelWork.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

#include "EntityWorld.hpp"
#include "EventBuffer.hpp"
#include "PhysicsBridge.hpp"
#include "SimulationRuntime.hpp"
#include "WorldComponents.hpp"

using namespace Runic;
using namespace Runic::Simulation;
using Core::Vec3;
using Core::WorldPosition;

namespace {

struct Seen {
    std::uint32_t visits = 0;
    std::uint32_t chunk = 0;
    std::uint32_t position = 0; // the entity's position in the loop
};

struct Tag {
    std::uint32_t value = 0;
};

ECS::EntityQuery<Seen> MakeSeen(SimulationRuntime &runtime, int count) {
    ECS::EntityWorld &entities = runtime.Entities();
    entities.RegisterComponent<Seen>("engine:test_seen");
    for (int i = 0; i < count; ++i) {
        entities.Set(entities.Create(), Seen{});
    }
    return entities.MakeQuery<Seen>("engine:test_seen_query");
}

// Runs master ticks until the realtime slot has run once more.
void RunRealtimeTick(SimulationRuntime &runtime) {
    const std::uint64_t before = runtime.TicksRun(TickDomain::Hz30);
    while (runtime.TicksRun(TickDomain::Hz30) == before) {
        runtime.RunMasterTick();
    }
}

std::uint64_t JobsRunByThePool(Core::WorkerPool &pool) {
    std::uint64_t jobs = pool.JobsRunInline();
    for (Core::WorkerIndex worker = 0; worker < pool.WorkerCount(); ++worker) {
        jobs += pool.JobsRunBy(worker);
    }
    return jobs;
}

template <class T> void Append(std::string &out, const T &value) {
    char bytes[sizeof(T)];
    std::memcpy(bytes, &value, sizeof(T));
    out.append(bytes, sizeof(T));
}

void AppendTransform(std::string &out, const World::WorldTransform &transform) {
    Append(out, transform.position.x);
    Append(out, transform.position.y);
    Append(out, transform.position.z);
    Append(out, transform.rotation.w);
    Append(out, transform.rotation.x);
    Append(out, transform.rotation.y);
    Append(out, transform.rotation.z);
}

// ---- The scene ----------------------------------------------------------------------------------
//
// Thousands of movers that drift and bounce inside a box and hit each other: every hit lowers its
// target's health in a way that depends on the order hits arrive in, and is a fact the Queries
// phase reads; a mover whose health runs out is destroyed, and now and then a mover spawns
// another. Beside them, crates fall on a floor and characters walk among them through the
// physics bridge, with their moves set from a parallel loop.

struct Mover {
    Vec3 velocity{0.0f};
    float health = 0.0f;
    std::uint32_t hits = 0;
};

struct Hit {
    ECS::Entity target;
    float amount = 0.0f;
};

struct Spawn {
    WorldPosition at;
    Vec3 velocity{0.0f};
};

constexpr int kMovers = 3'000;
constexpr int kMoverRows = 60;
constexpr double kMoverSpacingM = 2.0;
constexpr double kBoxHalfM = 70.0;
constexpr float kStartHealth = 100.0f;
constexpr float kHealthKept = 0.97f;
constexpr float kDeathHealth = 55.0f;
constexpr std::uint64_t kHitEvery = 7;
constexpr std::uint64_t kSpawnEvery = 997;
constexpr int kCrates = 100;
constexpr int kCharacters = 6;
constexpr float kWalkMps = 2.0f;
constexpr int kTwoSeconds = 60; // realtime ticks

const WorldPosition kOrigin{2'000'000.0, 15.0, 4'000'000.0};

// A number that looks random but depends only on its inputs.
std::uint64_t Mix(std::uint64_t a, std::uint64_t b) {
    std::uint64_t x = a * 0x9E3779B97F4A7C15ull ^ (b + 0x632BE59BD9B4E019ull);
    x ^= x >> 31;
    x *= 0xBF58476D1CE4E5B9ull;
    x ^= x >> 29;
    return x;
}

struct Outcome {
    std::string state; // every entity's values at the end, as bytes
    std::string facts; // every hit and contact fact the Queries phase read, with its ID
    std::uint64_t hits = 0, spawns = 0, deaths = 0, contacts = 0;
};

Outcome RunScene(std::uint32_t workers) {
    SimulationRuntime runtime({.workers = workers});
    ECS::EntityWorld &entities = runtime.Entities();
    entities.RegisterComponent<Mover>("engine:test_mover");
    PhysicsBridge &bridge = runtime.EnablePhysics();
    Physics::PhysicsWorld &physics = bridge.World();
    Events::EventBuffer<Hit> hitFacts(runtime.EventHub(), "engine:test_hits");

    std::vector<ECS::Entity> targets;
    for (int i = 0; i < kMovers; ++i) {
        const ECS::Entity mover = entities.Create();
        const double x = (i % kMoverRows) * kMoverSpacingM - kBoxHalfM / 2.0;
        const double z = (i / kMoverRows) * kMoverSpacingM - kBoxHalfM / 2.0;
        entities.Set(mover,
                     World::WorldTransform{{kOrigin.x + x, kOrigin.y + 50.0, kOrigin.z + z}});
        const std::uint64_t mix = Mix(static_cast<std::uint64_t>(i), 0);
        const Vec3 velocity(static_cast<float>(mix % 9) - 4.0f, 0.0f,
                            static_cast<float>((mix >> 8) % 9) - 4.0f);
        entities.Set(mover, Mover{velocity, kStartHealth, 0});
        targets.push_back(mover);
    }

    Physics::BodyDesc floor;
    floor.shape =
        physics.AddShape("engine:test_floor", Physics::BoxShape(Vec3(40.0f, 1.0f, 40.0f))).Value();
    floor.layer = physics.Layers().Find(Physics::kStaticLayerName);
    const ECS::Entity floorEntity = entities.Create();
    entities.Set(floorEntity, World::WorldTransform{{kOrigin.x, kOrigin.y - 1.0, kOrigin.z}});
    bridge.AddBody(floorEntity, floor);
    Physics::BodyDesc crate;
    crate.shape = physics.AddShape("engine:test_crate", Physics::BoxShape(Vec3(0.4f))).Value();
    crate.motion = Physics::MotionType::Dynamic;
    crate.layer = physics.Layers().Find(Physics::kMovingLayerName);
    std::vector<ECS::Entity> physical{floorEntity};
    for (int i = 0; i < kCrates; ++i) {
        const ECS::Entity entity = entities.Create();
        const double x = (i % 10) * 0.9 - 4.0 + (i / 50) * 0.3;
        const double y = 1.0 + (i / 10) * 0.9;
        const double z = ((i / 10) % 5) * 0.9 - 2.0;
        entities.Set(entity, World::WorldTransform{{kOrigin.x + x, kOrigin.y + y, kOrigin.z + z}});
        bridge.AddBody(entity, crate);
        physical.push_back(entity);
    }
    Physics::CharacterDesc walker;
    walker.layer = physics.Layers().Find(Physics::kCharacterLayerName);
    for (int i = 0; i < kCharacters; ++i) {
        const ECS::Entity entity = entities.Create();
        entities.Set(entity, World::WorldTransform{
                                 {kOrigin.x - 12.0, kOrigin.y, kOrigin.z - 7.5 + i * 3.0}});
        bridge.AddCharacter(entity, walker);
        physical.push_back(entity);
    }

    const auto movers = entities.MakeQuery<World::WorldTransform, Mover>("engine:test_movers");
    const auto walkers =
        entities.MakeQuery<PhysicsCharacter, const World::WorldTransform>("engine:test_walkers");
    ParallelOutput<Hit> hits(runtime);
    ParallelOutput<Spawn> spawns(runtime);
    ParallelOutput<ECS::Entity> deaths(runtime);
    Outcome outcome;

    runtime.AddSystem(
        {"engine:test_parallel_movement", ScheduleSlot::Hz30Realtime,
         [&](const TickContext &tick) {
             ParallelEach(
                 runtime, movers, kDefaultEntitiesPerChunk,
                 [&](const ParallelChunk &chunk, ECS::Entity entity,
                     World::WorldTransform &transform, Mover &mover) {
                     WorldPosition &at = transform.position;
                     at.x += static_cast<double>(mover.velocity.x * tick.dt);
                     at.z += static_cast<double>(mover.velocity.z * tick.dt);
                     if (std::abs(at.x - kOrigin.x) > kBoxHalfM) {
                         mover.velocity.x = -mover.velocity.x;
                     }
                     if (std::abs(at.z - kOrigin.z) > kBoxHalfM) {
                         mover.velocity.z = -mover.velocity.z;
                     }
                     const std::uint64_t mix = Mix(entity.Raw(), tick.domainTick);
                     if (mix % kHitEvery == 0) {
                         const float amount = 1.0f + static_cast<float>((mix >> 16) % 13) * 0.25f;
                         hits.Push(chunk, Hit{targets[(mix >> 24) % targets.size()], amount});
                     }
                     if (mix % kSpawnEvery == 0) {
                         spawns.Push(chunk, Spawn{at, -mover.velocity});
                     }
                     if (mover.health < kDeathHealth) {
                         deaths.Push(chunk, entity);
                     }
                 });
             hits.Merge([&](Hit &hit) {
                 if (!entities.IsAlive(hit.target)) {
                     return;
                 }
                 Mover &target = entities.Get<Mover>(hit.target);
                 target.health = target.health * kHealthKept - hit.amount;
                 ++target.hits;
                 hitFacts.Emit(hit);
                 ++outcome.hits;
             });
             spawns.Merge([&](Spawn &spawn) {
                 const ECS::Entity born = entities.Create();
                 entities.Set(born, World::WorldTransform{spawn.at});
                 entities.Set(born, Mover{spawn.velocity, kStartHealth, 0});
                 ++outcome.spawns;
             });
             deaths.Merge([&](ECS::Entity &dead) {
                 entities.Destroy(dead);
                 ++outcome.deaths;
             });
             // The walkers' moves, also from a parallel loop.
             ParallelEach(runtime, walkers, 1,
                          [&](const ParallelChunk &, ECS::Entity entity,
                              PhysicsCharacter &character, const World::WorldTransform &) {
                              const float turn =
                                  (entity.Raw() + tick.domainTick) % 40 < 20 ? 1.0f : -1.0f;
                              character.move.velocity = Vec3(kWalkMps, 0.0f, turn * kWalkMps);
                          });
         },
         RealtimePhase::Movement});

    Events::EventReader<Hit> readHits = hitFacts.MakeReader();
    Events::EventReader<PhysicsContact> readContacts = bridge.Contacts().MakeReader();
    runtime.AddSystem({"engine:test_read_facts", ScheduleSlot::Hz30Realtime,
                       [&](const TickContext &) {
                           for (const Events::Event<Hit> &hit : readHits.Take()) {
                               Append(outcome.facts, hit.id.sequence);
                               Append(outcome.facts, hit.fact.target.Raw());
                               Append(outcome.facts, hit.fact.amount);
                           }
                           for (const Events::Event<PhysicsContact> &contact :
                                readContacts.Take()) {
                               Append(outcome.facts, contact.id.sequence);
                               Append(outcome.facts, contact.fact.phase);
                               Append(outcome.facts, contact.fact.a.Raw());
                               Append(outcome.facts, contact.fact.b.Raw());
                               Append(outcome.facts, contact.fact.point.x);
                               Append(outcome.facts, contact.fact.point.y);
                               Append(outcome.facts, contact.fact.point.z);
                               ++outcome.contacts;
                           }
                       },
                       RealtimePhase::Queries});

    for (int tick = 0; tick < kTwoSeconds; ++tick) {
        RunRealtimeTick(runtime);
    }

    movers.Each(
        [&](ECS::Entity entity, const World::WorldTransform &transform, const Mover &mover) {
            Append(outcome.state, entity.Raw());
            AppendTransform(outcome.state, transform);
            Append(outcome.state, mover.velocity.x);
            Append(outcome.state, mover.velocity.z);
            Append(outcome.state, mover.health);
            Append(outcome.state, mover.hits);
        });
    for (const ECS::Entity entity : physical) {
        AppendTransform(outcome.state, entities.Get<World::WorldTransform>(entity));
    }
    return outcome;
}

} // namespace

TEST(ParallelEach, VisitsEveryEntityOnceInChunksThatDependOnlyOnTheCount) {
    constexpr int kEntities = 1'000;
    constexpr std::uint32_t kPerChunk = 64;
    for (const std::uint32_t workers : {1u, 4u}) {
        SimulationRuntime runtime({.workers = workers});
        const ECS::EntityQuery<Seen> seen = MakeSeen(runtime, kEntities);
        ParallelEach(runtime, seen, kPerChunk,
                     [](const ParallelChunk &chunk, ECS::Entity, Seen &value) {
                         ++value.visits;
                         value.chunk = chunk.chunk;
                     });
        std::uint32_t position = 0;
        seen.Each([&](ECS::Entity, const Seen &value) {
            EXPECT_EQ(value.visits, 1u);
            EXPECT_EQ(value.chunk, position / kPerChunk) << "with " << workers << " workers";
            ++position;
        });
        EXPECT_EQ(position, static_cast<std::uint32_t>(kEntities));
    }
}

TEST(ParallelEach, SpansRunsOfEntitiesStoredApart) {
    // Entities with different sets of components are stored apart; a chunk may cross from one run
    // into the next, and still sees every entity once, in the query's order.
    SimulationRuntime runtime({.workers = 4});
    ECS::EntityWorld &entities = runtime.Entities();
    const ECS::EntityQuery<Seen> seen = MakeSeen(runtime, 100);
    entities.RegisterComponent<Tag>("engine:test_tag");
    for (int i = 0; i < 150; ++i) {
        const ECS::Entity entity = entities.Create();
        entities.Set(entity, Seen{});
        entities.Set(entity, Tag{static_cast<std::uint32_t>(i)});
    }
    ParallelEach(runtime, seen, 32, [](const ParallelChunk &chunk, ECS::Entity, Seen &value) {
        ++value.visits;
        value.position = chunk.begin;
    });
    std::uint32_t position = 0;
    seen.Each([&](ECS::Entity, const Seen &value) {
        EXPECT_EQ(value.visits, 1u);
        EXPECT_EQ(value.position, position / 32 * 32);
        ++position;
    });
    EXPECT_EQ(position, 250u);
}

TEST(ParallelOutput, MergesByLoopThenChunkThenPushOrder) {
    SimulationRuntime runtime({.workers = 4});
    const ECS::EntityQuery<Seen> seen = MakeSeen(runtime, 500);
    ParallelOutput<std::pair<std::uint64_t, int>> out(runtime);
    ParallelEach(runtime, seen, 16, [&](const ParallelChunk &chunk, ECS::Entity entity, Seen &) {
        out.Push(chunk, {entity.Raw(), 0});
        out.Push(chunk, {entity.Raw(), 1});
    });
    ParallelEach(runtime, seen, 7, [&](const ParallelChunk &chunk, ECS::Entity entity, Seen &) {
        out.Push(chunk, {entity.Raw(), 2});
    });
    EXPECT_EQ(out.Size(), 1'500u);

    std::vector<std::pair<std::uint64_t, int>> expected;
    seen.Each([&](ECS::Entity entity, const Seen &) {
        expected.emplace_back(entity.Raw(), 0);
        expected.emplace_back(entity.Raw(), 1);
    });
    seen.Each([&](ECS::Entity entity, const Seen &) { expected.emplace_back(entity.Raw(), 2); });
    std::vector<std::pair<std::uint64_t, int>> merged;
    out.Merge([&](std::pair<std::uint64_t, int> &value) { merged.push_back(value); });
    EXPECT_EQ(merged, expected);
    EXPECT_EQ(out.Size(), 0u);
}

TEST(ParallelPhases, TheSameSceneWithOneWorkerAndWithManyGivesIdenticalResults) {
    const Outcome alone = RunScene(Core::kSingleWorker);
    // The scene does what it is meant to: hits land, movers die and are born, crates touch.
    EXPECT_GT(alone.hits, 1'000u);
    EXPECT_GT(alone.deaths, 0u);
    EXPECT_GT(alone.spawns, 0u);
    EXPECT_GT(alone.contacts, static_cast<std::uint64_t>(kCrates));
    for (const std::uint32_t workers : {2u, 4u, 8u}) {
        const Outcome many = RunScene(workers);
        EXPECT_EQ(many.hits, alone.hits) << workers << " workers";
        EXPECT_EQ(many.deaths, alone.deaths) << workers << " workers";
        EXPECT_EQ(many.spawns, alone.spawns) << workers << " workers";
        EXPECT_EQ(many.contacts, alone.contacts) << workers << " workers";
        EXPECT_TRUE(many.state == alone.state)
            << "the entities differ with " << workers << " workers";
        EXPECT_TRUE(many.facts == alone.facts) << "the facts differ with " << workers << " workers";
    }
}

TEST(PhysicsBridge, ThePhysicsStepRunsOnTheRuntimesPool) {
    SimulationRuntime runtime({.workers = 4});
    PhysicsBridge &bridge = runtime.EnablePhysics();
    Physics::BodyDesc crate;
    crate.shape =
        bridge.World().AddShape("engine:test_crate", Physics::BoxShape(Vec3(0.5f))).Value();
    crate.motion = Physics::MotionType::Dynamic;
    crate.layer = bridge.World().Layers().Find(Physics::kMovingLayerName);
    for (int i = 0; i < 20; ++i) {
        const ECS::Entity entity = runtime.Entities().Create();
        runtime.Entities().Set(entity, World::WorldTransform{{i * 2.0, 0.0, 0.0}});
        bridge.AddBody(entity, crate);
    }
    const std::uint64_t before = JobsRunByThePool(runtime.Workers());
    for (int tick = 0; tick < 10; ++tick) {
        RunRealtimeTick(runtime);
    }
    // Jolt queued its step's jobs on the runtime's pool every realtime tick.
    EXPECT_GE(JobsRunByThePool(runtime.Workers()), before + 10u);
}

#if RUNIC_ASSERTS_ENABLED

TEST(ParallelWorkDeathTest, ALoopInsideAChunkOrAMergeDuringALoopIsAFault) {
    SimulationRuntime runtime({.workers = Core::kSingleWorker});
    const ECS::EntityQuery<Seen> seen = MakeSeen(runtime, 4);
    ParallelOutput<int> out(runtime);
    EXPECT_DEATH(ParallelEach(runtime, seen, 1,
                              [&](const ParallelChunk &, ECS::Entity, Seen &) {
                                  ParallelEach(runtime, seen, 1,
                                               [](const ParallelChunk &, ECS::Entity, Seen &) {});
                              }),
                 "inside another parallel loop");
    EXPECT_DEATH(
        ParallelEach(runtime, seen, 1,
                     [&](const ParallelChunk &, ECS::Entity, Seen &) { out.Merge([](int &) {}); }),
        "merged while a parallel loop runs");
    EXPECT_DEATH(ParallelEach(runtime, seen, 0, [](const ParallelChunk &, ECS::Entity, Seen &) {}),
                 "at least one entity per chunk");
}

TEST(ParallelWorkDeathTest, PhysicsStepsOnlyOnTheRuntimesPool) {
    SimulationRuntime runtime;
    Core::WorkerPool other;
    Physics::PhysicsWorldSpec spec;
    spec.workers = &other;
    EXPECT_DEATH(runtime.EnablePhysics(spec), "the runtime's worker pool");
}

#endif
