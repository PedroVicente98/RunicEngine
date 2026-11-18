// The server's parallel demo (ParallelDemo.hpp).

#include "ParallelDemo.hpp"

#include <cmath>
#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>

#include "EntityWorld.hpp"
#include "EventBuffer.hpp"
#include "Log.hpp"
#include "ParallelWork.hpp"
#include "PhysicsBridge.hpp"
#include "WorldComponents.hpp"

namespace Runic::Programs {

namespace {

constexpr std::string_view kMoverName = "engine:demo_mover";
constexpr std::string_view kMoversQueryName = "engine:demo_movers";
constexpr std::string_view kWalkersQueryName = "engine:demo_walkers";
constexpr std::string_view kHitsBufferName = "engine:demo_hits";
constexpr std::string_view kMoveSystemName = "engine:demo_parallel_movement";
constexpr std::string_view kReadSystemName = "engine:demo_read_hits";
constexpr std::string_view kReportSystemName = "engine:demo_report";
constexpr std::string_view kFloorShapeName = "engine:demo_floor";
constexpr std::string_view kCrateShapeName = "engine:demo_crate";

// The scene, about 4,500 km from the world's origin.
const Core::WorldPosition kOrigin{2'000'000.0, 15.0, 4'000'000.0};
constexpr int kMovers = 3'000;
constexpr int kMoverRows = 60;
constexpr double kMoverSpacingM = 2.0;
constexpr double kMoverHeightM = 50.0;
constexpr double kBoxHalfM = 70.0;
constexpr std::uint64_t kStartSeed = 0;  // what a mover's starting speed is mixed with
constexpr std::uint64_t kSpeedSteps = 9; // a mover's speed on each axis: -4 to 4 m/s
constexpr float kSpeedOffsetMps = 4.0f;
constexpr float kNoVerticalSpeed = 0.0f;
constexpr std::uint32_t kNoHits = 0;
constexpr int kSecondAxisShift = 8;
constexpr float kStartHealth = 100.0f;
constexpr float kHealthKept = 0.97f; // a hit keeps this much of the health, then takes its amount
constexpr float kDeathHealth = 55.0f;
constexpr std::uint64_t kHitEvery = 7;     // about one mover in this many hits someone each tick
constexpr std::uint64_t kSpawnEvery = 997; // and one in this many spawns another
constexpr int kAmountShift = 16;
constexpr std::uint64_t kAmountSteps = 13;
constexpr float kMinHitAmount = 1.0f;
constexpr float kHitAmountStep = 0.25f;
constexpr int kTargetShift = 24;

constexpr int kCrates = 100;
constexpr int kCratesPerRow = 10;
constexpr int kCrateRowsPerLayer = 5;
constexpr int kCratesPerLayer = kCratesPerRow * kCrateRowsPerLayer;
constexpr double kCrateSpacingM = 0.9;
constexpr double kCrateRowOffsetM = 4.0;
constexpr double kCrateLayerShiftM = 0.3;
constexpr double kCrateDepthOffsetM = 2.0;
constexpr double kCrateFirstHeightM = 1.0;
constexpr float kCrateHalfM = 0.4f;
const Core::Vec3 kFloorHalfM{40.0f, 1.0f, 40.0f};
constexpr double kFloorDepthM = 1.0;

constexpr int kCharacters = 6;
constexpr double kCharacterStartXM = -12.0;
constexpr double kCharacterFirstZM = -7.5;
constexpr double kCharacterSpacingM = 3.0;
constexpr float kWalkMps = 2.0f;
constexpr std::uint64_t kTurnPeriodTicks = 40; // a walker zigzags: half the period each way
constexpr float kOneWay = 1.0f;
constexpr float kOtherWay = -1.0f;
constexpr std::uint32_t kWalkersPerChunk = 1;

// When the server reports the scene's hash.
constexpr std::uint64_t kReportTick = 60;

// FNV-1a over the bytes of the values: two runs agree only if every value agrees to the bit.
constexpr std::uint64_t kFnvOffset = 0xCBF29CE484222325ull;
constexpr std::uint64_t kFnvPrime = 0x100000001B3ull;

// A number that looks random but depends only on its inputs.
constexpr std::uint64_t kMixGolden = 0x9E3779B97F4A7C15ull;
constexpr std::uint64_t kMixSalt = 0x632BE59BD9B4E019ull;
constexpr std::uint64_t kMixMultiplier = 0xBF58476D1CE4E5B9ull;
constexpr int kMixFirstShift = 31;
constexpr int kMixSecondShift = 29;

std::uint64_t Mix(std::uint64_t a, std::uint64_t b) {
    std::uint64_t x = a * kMixGolden ^ (b + kMixSalt);
    x ^= x >> kMixFirstShift;
    x *= kMixMultiplier;
    x ^= x >> kMixSecondShift;
    return x;
}

class Fingerprint {
  public:
    template <class T> void Add(const T &value) {
        unsigned char bytes[sizeof(T)];
        std::memcpy(bytes, &value, sizeof(T));
        for (const unsigned char byte : bytes) {
            m_Hash = (m_Hash ^ byte) * kFnvPrime;
        }
    }
    void Add(const World::WorldTransform &transform) {
        Add(transform.position.x);
        Add(transform.position.y);
        Add(transform.position.z);
        Add(transform.rotation.w);
        Add(transform.rotation.x);
        Add(transform.rotation.y);
        Add(transform.rotation.z);
    }
    std::uint64_t Value() const noexcept { return m_Hash; }

  private:
    std::uint64_t m_Hash = kFnvOffset;
};

struct Mover {
    Core::Vec3 velocity{0.0f};
    float health = 0.0f;
    std::uint32_t hits = 0;
};

struct Hit {
    ECS::Entity target;
    float amount = 0.0f;
};

struct Spawn {
    Core::WorldPosition at;
    Core::Vec3 velocity{0.0f};
};

// What the demo's systems share. The hits' buffer and reader go with the systems, before the
// runtime's hub.
struct DemoState {
    explicit DemoState(Simulation::SimulationRuntime &runtime)
        : hits(runtime), spawns(runtime), deaths(runtime),
          hitFacts(runtime.EventHub(), kHitsBufferName), readHits(hitFacts.MakeReader()) {}

    std::vector<ECS::Entity> targets;
    std::vector<ECS::Entity> physical;
    ECS::EntityQuery<World::WorldTransform, Mover> movers;
    ECS::EntityQuery<Simulation::PhysicsCharacter, const World::WorldTransform> walkers;
    Simulation::ParallelOutput<Hit> hits;
    Simulation::ParallelOutput<Spawn> spawns;
    Simulation::ParallelOutput<ECS::Entity> deaths;
    Events::EventBuffer<Hit> hitFacts;
    Events::EventReader<Hit> readHits;
    Fingerprint facts;
    std::uint64_t hitCount = 0;
    std::uint64_t spawned = 0;
    std::uint64_t died = 0;
};

void BuildScene(Simulation::SimulationRuntime &runtime, DemoState &state) {
    ECS::EntityWorld &entities = runtime.Entities();
    for (int i = 0; i < kMovers; ++i) {
        const ECS::Entity mover = entities.Create();
        const double x = (i % kMoverRows) * kMoverSpacingM - kBoxHalfM / 2.0;
        const double z = (i / kMoverRows) * kMoverSpacingM - kBoxHalfM / 2.0;
        entities.Set(mover, World::WorldTransform{
                                {kOrigin.x + x, kOrigin.y + kMoverHeightM, kOrigin.z + z}});
        const std::uint64_t mix = Mix(static_cast<std::uint64_t>(i), kStartSeed);
        const Core::Vec3 velocity(
            static_cast<float>(mix % kSpeedSteps) - kSpeedOffsetMps, kNoVerticalSpeed,
            static_cast<float>((mix >> kSecondAxisShift) % kSpeedSteps) - kSpeedOffsetMps);
        entities.Set(mover, Mover{velocity, kStartHealth, kNoHits});
        state.targets.push_back(mover);
    }

    Simulation::PhysicsBridge &bridge = runtime.EnablePhysics();
    Physics::PhysicsWorld &physics = bridge.World();
    Physics::BodyDesc floor;
    floor.shape = physics.AddShape(kFloorShapeName, Physics::BoxShape(kFloorHalfM)).Value();
    floor.layer = physics.Layers().Find(Physics::kStaticLayerName);
    const ECS::Entity floorEntity = entities.Create();
    entities.Set(floorEntity,
                 World::WorldTransform{{kOrigin.x, kOrigin.y - kFloorDepthM, kOrigin.z}});
    bridge.AddBody(floorEntity, floor);
    state.physical.push_back(floorEntity);

    Physics::BodyDesc crate;
    crate.shape =
        physics.AddShape(kCrateShapeName, Physics::BoxShape(Core::Vec3(kCrateHalfM))).Value();
    crate.motion = Physics::MotionType::Dynamic;
    crate.layer = physics.Layers().Find(Physics::kMovingLayerName);
    for (int i = 0; i < kCrates; ++i) {
        const ECS::Entity entity = entities.Create();
        const double x = (i % kCratesPerRow) * kCrateSpacingM - kCrateRowOffsetM +
                         (i / kCratesPerLayer) * kCrateLayerShiftM;
        const double y = kCrateFirstHeightM + (i / kCratesPerRow) * kCrateSpacingM;
        const double z =
            ((i / kCratesPerRow) % kCrateRowsPerLayer) * kCrateSpacingM - kCrateDepthOffsetM;
        entities.Set(entity, World::WorldTransform{{kOrigin.x + x, kOrigin.y + y, kOrigin.z + z}});
        bridge.AddBody(entity, crate);
        state.physical.push_back(entity);
    }

    Physics::CharacterDesc walker;
    walker.layer = physics.Layers().Find(Physics::kCharacterLayerName);
    for (int i = 0; i < kCharacters; ++i) {
        const ECS::Entity entity = entities.Create();
        entities.Set(entity, World::WorldTransform{
                                 {kOrigin.x + kCharacterStartXM, kOrigin.y,
                                  kOrigin.z + kCharacterFirstZM + i * kCharacterSpacingM}});
        bridge.AddCharacter(entity, walker);
        state.physical.push_back(entity);
    }
}

// Movement: the movers in chunks across the workers, what they do to others merged in chunk
// order, then the walkers' moves.
void Move(Simulation::SimulationRuntime &runtime, DemoState &state,
          const Simulation::TickContext &tick) {
    ECS::EntityWorld &entities = runtime.Entities();
    Simulation::ParallelEach(
        runtime, state.movers, Simulation::kDefaultEntitiesPerChunk,
        [&](const Simulation::ParallelChunk &chunk, ECS::Entity entity,
            World::WorldTransform &transform, Mover &mover) {
            Core::WorldPosition &at = transform.position;
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
                const float amount =
                    kMinHitAmount +
                    static_cast<float>((mix >> kAmountShift) % kAmountSteps) * kHitAmountStep;
                state.hits.Push(
                    chunk,
                    Hit{state.targets[(mix >> kTargetShift) % state.targets.size()], amount});
            }
            if (mix % kSpawnEvery == 0) {
                state.spawns.Push(chunk, Spawn{at, -mover.velocity});
            }
            if (mover.health < kDeathHealth) {
                state.deaths.Push(chunk, entity);
            }
        });
    state.hits.Merge([&](Hit &hit) {
        if (!entities.IsAlive(hit.target)) {
            return;
        }
        Mover &target = entities.Get<Mover>(hit.target);
        target.health = target.health * kHealthKept - hit.amount;
        ++target.hits;
        state.hitFacts.Emit(hit);
        ++state.hitCount;
    });
    state.spawns.Merge([&](Spawn &spawn) {
        const ECS::Entity born = entities.Create();
        entities.Set(born, World::WorldTransform{spawn.at});
        entities.Set(born, Mover{spawn.velocity, kStartHealth, kNoHits});
        ++state.spawned;
    });
    state.deaths.Merge([&](ECS::Entity &dead) {
        entities.Destroy(dead);
        ++state.died;
    });
    Simulation::ParallelEach(
        runtime, state.walkers, kWalkersPerChunk,
        [&](const Simulation::ParallelChunk &, ECS::Entity entity,
            Simulation::PhysicsCharacter &character, const World::WorldTransform &) {
            const bool oneWay =
                (entity.Raw() + tick.domainTick) % kTurnPeriodTicks < kTurnPeriodTicks / 2;
            character.move.velocity =
                Core::Vec3(kWalkMps, kNoVerticalSpeed, (oneWay ? kOneWay : kOtherWay) * kWalkMps);
        });
}

void Report(Simulation::SimulationRuntime &runtime, DemoState &state) {
    const ECS::EntityWorld &entities = runtime.Entities();
    Fingerprint scene;
    std::uint32_t movers = 0;
    state.movers.Each(
        [&](ECS::Entity entity, const World::WorldTransform &transform, const Mover &mover) {
            scene.Add(entity.Raw());
            scene.Add(transform);
            scene.Add(mover.velocity.x);
            scene.Add(mover.velocity.z);
            scene.Add(mover.health);
            scene.Add(mover.hits);
            ++movers;
        });
    for (const ECS::Entity entity : state.physical) {
        scene.Add(entities.Get<World::WorldTransform>(entity));
    }
    scene.Add(state.facts.Value());
    RUNIC_LOG_INFO("RUNIC: parallel demo at realtime tick {} on {} worker(s): state {:016x}; "
                   "{} movers, {} hits, {} died, {} spawned, {} crates and {} walkers in physics",
                   kReportTick, runtime.Workers().WorkerCount(), scene.Value(), movers,
                   state.hitCount, state.died, state.spawned, kCrates, kCharacters);
}

} // namespace

void AddParallelDemo(Simulation::SimulationRuntime &runtime) {
    ECS::EntityWorld &entities = runtime.Entities();
    entities.RegisterComponent<Mover>(kMoverName);
    auto state = std::make_shared<DemoState>(runtime);
    BuildScene(runtime, *state);
    state->movers = entities.MakeQuery<World::WorldTransform, Mover>(kMoversQueryName);
    state->walkers = entities.MakeQuery<Simulation::PhysicsCharacter, const World::WorldTransform>(
        kWalkersQueryName);

    runtime.AddSystem(
        {kMoveSystemName, Simulation::ScheduleSlot::Hz30Realtime,
         [&runtime, state](const Simulation::TickContext &tick) { Move(runtime, *state, tick); },
         Simulation::RealtimePhase::Movement});
    runtime.AddSystem({kReadSystemName, Simulation::ScheduleSlot::Hz30Realtime,
                       [state](const Simulation::TickContext &) {
                           for (const Events::Event<Hit> &hit : state->readHits.Take()) {
                               state->facts.Add(hit.id.sequence);
                               state->facts.Add(hit.fact.target.Raw());
                               state->facts.Add(hit.fact.amount);
                           }
                       },
                       Simulation::RealtimePhase::Queries});
    runtime.AddSystem({kReportSystemName, Simulation::ScheduleSlot::Hz30Realtime,
                       [&runtime, state](const Simulation::TickContext &tick) {
                           if (tick.domainTick == kReportTick) {
                               Report(runtime, *state);
                           }
                       },
                       Simulation::RealtimePhase::Commit});
}

} // namespace Runic::Programs
