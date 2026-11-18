// RunicClient's spatial scene (SpatialScene.hpp).

#include "SpatialScene.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <format>
#include <random>
#include <string>
#include <vector>

#include <glm/ext/scalar_constants.hpp>

#include "AuthoredVolumes.hpp"
#include "EntityWorld.hpp"
#include "Log.hpp"
#include "RenderData/DebugDraw.hpp"
#include "SpatialDebug.hpp"
#include "SpatialIndex.hpp"
#include "Time.hpp"
#include "WorldComponents.hpp"

namespace Runic::Programs {

namespace {

using Core::Vec3;
using Core::WorldPosition;

constexpr double kTwoPi = glm::two_pi<double>();

// The scene, 6,400 km out (its ground at the origin's height).
const WorldPosition kCenter{4'000'000.0, 0.0, -5'000'000.0};

WorldPosition At(double x, double y, double z) {
    return {kCenter.x + x, kCenter.y + y, kCenter.z + z};
}

// The walkers: a 200 m box from the cave's floor to above the island.
constexpr std::uint32_t kWalkers = 150;
constexpr double kBoxHalfM = 100.0;
constexpr double kLowestM = -60.0;
constexpr double kHighestM = 110.0;
constexpr float kMaxGroundSpeedMps = 6.0f;
constexpr float kMaxClimbMps = 3.0f;
constexpr std::uint32_t kRandomSeed = 11;
constexpr std::string_view kWalkerName = "engine:demo_spatial_walker";
constexpr std::string_view kWalkersQueryName = "engine:demo_spatial_walkers";
constexpr std::string_view kMoveSystemName = "engine:demo_spatial_move";
constexpr std::string_view kMembershipSystemName = "engine:demo_spatial_membership";
constexpr std::string_view kQuerySystemName = "engine:demo_spatial_query";

struct Walker {
    Vec3 velocity{0.0f};
};

// The query sweeps a circle 50 m wide, rising from the cave to the island and back every 20 s,
// and changes shape every 2 s.
constexpr double kQueryCircleM = 50.0;
constexpr double kQueryLapS = 12.0;
constexpr double kQuerySweepS = 20.0;
constexpr double kQueryMiddleM = 25.0;
constexpr double kQuerySwingM = 65.0;
constexpr double kShapeEveryS = 2.0;
constexpr int kSphereTurn = 0;
constexpr int kBoxTurn = 1;
constexpr int kShapes = 3; // then the ellipsoid's turn
constexpr float kQueryRadiusM = 25.0f;
const Vec3 kQueryBoxHalfM(20.0f, 10.0f, 20.0f);
const Vec3 kQueryEllipsoidM(40.0f, 12.0f, 40.0f);

// The volumes.
constexpr std::string_view kCaveKindName = "engine:demo_cave";
constexpr std::string_view kAreaKindName = "engine:demo_quest_area";
constexpr std::string_view kIslandKindName = "engine:demo_island";
constexpr std::string_view kDeckKindName = "engine:demo_deck";
constexpr bool kNotSolid = false;
constexpr bool kFollowsEntity = true;
constexpr float kTunnelTurnRad = 0.44f; // 25° about the up axis
const Vec3 kTunnelHalfM(60.0f, 6.0f, 8.0f);
const Vec3 kTunnelAtM(-30.0f, -35.0f, 10.0f);
constexpr float kChamberRadiusM = 18.0f;
const Vec3 kChamberAtM(20.0f, -40.0f, -20.0f);
constexpr float kAreaRadiusM = 30.0f;
constexpr float kAreaHalfHeightM = 15.0f;
const Vec3 kAreaAtM(50.0f, 0.0f, 50.0f);
constexpr float kIslandRadiusM = 35.0f;
constexpr float kIslandHalfHeightM = 10.0f;
const Vec3 kIslandAtM(-50.0f, 90.0f, -50.0f);
const Vec3 kDeckHalfM(12.0f, 1.5f, 4.0f);
constexpr double kDeckCircleM = 70.0;
constexpr double kDeckLapS = 30.0;
constexpr double kDeckHeightM = 2.0;
constexpr double kHalf = 0.5;
constexpr float kNoTilt = 0.0f; // the x and z parts of a turn about the up axis

// The camera circles the scene once a minute from above.
constexpr double kCameraOrbitPeriodS = 60.0;
constexpr double kCameraOrbitRadiusM = 230.0;
constexpr double kCameraHeightM = 130.0;
constexpr double kViewRadiusM = 420.0; // from the camera: the whole scene
const Vec3 kLabelAbove(0.0f, 4.0f, 0.0f);

Core::Quat AboutUp(double radians) {
    return {static_cast<float>(std::cos(radians * kHalf)), kNoTilt,
            static_cast<float>(std::sin(radians * kHalf)), kNoTilt};
}

const char *ShapeName(World::QueryShape shape) {
    switch (shape) {
    case World::QueryShape::Box:
        return "box";
    case World::QueryShape::Ellipsoid:
        return "ellipsoid";
    case World::QueryShape::Sphere:
        break;
    }
    return "sphere";
}

} // namespace

struct SpatialSceneLayer::State {
    State(Simulation::SimulationRuntime &simulation, Client::ClientCamera &view)
        : runtime(simulation), camera(view) {}

    Simulation::SimulationRuntime &runtime;
    Client::ClientCamera &camera;
    World::SpatialIndex index;
    World::VolumeSet volumes;
    ECS::EntityQuery<World::WorldTransform, Walker> walkers;
    std::vector<ECS::Entity> all;
    World::VolumeId cave;
    World::VolumeId deck;
    World::SpatialQuery query = World::SpatialQuery::Sphere(kCenter, kQueryRadiusM);
    std::vector<ECS::Entity> found;
    std::vector<ECS::Entity> sorted;
    std::vector<ECS::Entity> expected;
    std::vector<World::VolumeId> at;
    std::uint64_t queries = 0;
    std::uint64_t foundTotal = 0;
    std::uint64_t differed = 0;
    std::uint64_t inCave = 0;

    void AddVolumes();
    void Move(const Simulation::TickContext &tick);
    void UpdateMembership(const Simulation::TickContext &tick);
    void RunQuery(const Simulation::TickContext &tick);
};

void SpatialSceneLayer::State::AddVolumes() {
    const World::VolumeKindId caveKind = volumes.AddKind({kCaveKindName}).Value();
    const World::VolumeKindId areaKind = volumes.AddKind({kAreaKindName}).Value();
    const World::VolumeKindId islandKind = volumes.AddKind({kIslandKindName}).Value();
    const World::VolumeKindId deckKind =
        volumes.AddKind({kDeckKindName, kNotSolid, kFollowsEntity}).Value();
    World::VolumeDesc desc;
    desc.kind = caveKind;
    desc.origin = kCenter;
    desc.parts = {World::BoxPart(kTunnelHalfM, kTunnelAtM, AboutUp(kTunnelTurnRad)),
                  World::SpherePart(kChamberRadiusM, kChamberAtM)};
    cave = volumes.Add(desc).Value();
    desc.kind = areaKind;
    desc.parts = {World::CylinderPart(kAreaRadiusM, kAreaHalfHeightM, kAreaAtM)};
    volumes.Add(desc).Value();
    desc.kind = islandKind;
    desc.parts = {World::CylinderPart(kIslandRadiusM, kIslandHalfHeightM, kIslandAtM)};
    desc.indivisible = true;
    volumes.Add(desc).Value();
    desc.kind = deckKind;
    desc.origin = At(kDeckCircleM, kDeckHeightM, 0.0);
    desc.parts = {World::BoxPart(kDeckHalfM)};
    desc.indivisible = false;
    deck = volumes.Add(desc).Value();
}

void SpatialSceneLayer::State::Move(const Simulation::TickContext &tick) {
    walkers.Each([&](ECS::Entity, World::WorldTransform &transform, Walker &walker) {
        WorldPosition &p = transform.position;
        p = p + walker.velocity * tick.dt;
        if (std::abs(p.x - kCenter.x) > kBoxHalfM) {
            walker.velocity.x = -walker.velocity.x;
        }
        if (std::abs(p.z - kCenter.z) > kBoxHalfM) {
            walker.velocity.z = -walker.velocity.z;
        }
        if (p.y - kCenter.y < kLowestM || p.y - kCenter.y > kHighestM) {
            walker.velocity.y = -walker.velocity.y;
        }
    });
}

void SpatialSceneLayer::State::UpdateMembership(const Simulation::TickContext &tick) {
    // RunicDocs › Realtime 30 Hz phase: PostPhysics writes positions to spatial membership.
    walkers.Each([&](ECS::Entity entity, const World::WorldTransform &transform, const Walker &) {
        index.Update(entity, {transform.position});
        at.clear();
        volumes.VolumesAt(transform.position, at);
        inCave += static_cast<std::uint64_t>(std::count(at.begin(), at.end(), cave));
    });
    // The ship's deck goes round the scene, turning as it goes.
    const double lap = kTwoPi * Core::ToSeconds(tick.time) / kDeckLapS;
    volumes.SetPose(deck,
                    At(std::cos(lap) * kDeckCircleM, kDeckHeightM, std::sin(lap) * kDeckCircleM),
                    AboutUp(-lap));
}

void SpatialSceneLayer::State::RunQuery(const Simulation::TickContext &tick) {
    const double seconds = Core::ToSeconds(tick.time);
    const double lap = kTwoPi * seconds / kQueryLapS;
    const double sweep = kTwoPi * seconds / kQuerySweepS;
    const WorldPosition center =
        At(std::cos(lap) * kQueryCircleM, kQueryMiddleM + std::sin(sweep) * kQuerySwingM,
           std::sin(lap) * kQueryCircleM);
    switch (static_cast<int>(seconds / kShapeEveryS) % kShapes) {
    case kSphereTurn:
        query = World::SpatialQuery::Sphere(center, kQueryRadiusM);
        break;
    case kBoxTurn:
        query = World::SpatialQuery::Box(center, kQueryBoxHalfM);
        break;
    default:
        query = World::SpatialQuery::Ellipsoid(center, kQueryEllipsoidM);
        break;
    }
    found.clear();
    index.Query(query, found);
    // What testing every walker finds, to compare.
    expected.clear();
    for (const ECS::Entity walker : all) {
        if (query.Overlaps(index.BoundsOf(walker))) {
            expected.push_back(walker);
        }
    }
    sorted = found;
    std::sort(sorted.begin(), sorted.end());
    std::sort(expected.begin(), expected.end());
    if (sorted != expected) {
        ++differed;
    }
    ++queries;
    foundTotal += found.size();
}

namespace {

// Draws the volumes, the index and the query near the frame's origin.
class SpatialViewExtractor final : public Client::Extractor {
  public:
    SpatialViewExtractor(const World::SpatialIndex &index, const World::VolumeSet &volumes,
                         const World::SpatialQuery &query, const std::vector<ECS::Entity> &found)
        : m_Index(index), m_Volumes(volumes), m_Query(query), m_Found(found) {
        m_Settings.radiusM = kViewRadiusM;
    }

    void Extract(const Client::ExtractionView &view, RenderData::RenderFrame &frame) override {
        World::DrawVolumes(m_Volumes, view.origin, frame.debug, m_Settings);
        World::DrawSpatialIndex(m_Index, view.origin, frame.debug, m_Settings);
        World::DrawSpatialQuery(m_Index, m_Query, m_Found, view.origin, frame.debug);
        RenderData::DebugDraw overlay(frame.debug, RenderData::DebugBatch::Overlay);
        overlay.DrawText(Core::ToLocal(m_Query.center - view.origin) + kLabelAbove,
                         std::format("{}: {} found", ShapeName(m_Query.shape), m_Found.size()),
                         World::kSpatialQueryColor);
    }

  private:
    const World::SpatialIndex &m_Index;
    const World::VolumeSet &m_Volumes;
    const World::SpatialQuery &m_Query;
    const std::vector<ECS::Entity> &m_Found;
    World::SpatialDebugSettings m_Settings;
};

} // namespace

SpatialSceneLayer::SpatialSceneLayer(Simulation::SimulationRuntime &runtime,
                                     Client::ClientCamera &camera)
    : Layer(std::string(kSpatialSceneLayerName)),
      m_State(std::make_unique<State>(runtime, camera)) {
    State &state = *m_State;
    ECS::EntityWorld &entities = runtime.Entities();
    entities.RegisterComponent<Walker>(kWalkerName);
    std::mt19937 random(kRandomSeed);
    std::uniform_real_distribution<double> across(-kBoxHalfM, kBoxHalfM);
    std::uniform_real_distribution<double> height(kLowestM, kHighestM);
    std::uniform_real_distribution<float> ground(-kMaxGroundSpeedMps, kMaxGroundSpeedMps);
    std::uniform_real_distribution<float> climb(-kMaxClimbMps, kMaxClimbMps);
    for (std::uint32_t i = 0; i < kWalkers; ++i) {
        const ECS::Entity walker = entities.Create();
        const WorldPosition at = At(across(random), height(random), across(random));
        entities.Set(walker, World::WorldTransform{at});
        entities.Set(walker, Walker{Vec3(ground(random), climb(random), ground(random))});
        state.index.Insert(walker, {at});
        state.all.push_back(walker);
    }
    state.walkers = entities.MakeQuery<World::WorldTransform, Walker>(kWalkersQueryName);
    state.AddVolumes();
    runtime.AddSystem({kMoveSystemName, Simulation::ScheduleSlot::Hz30Realtime,
                       [&state](const Simulation::TickContext &tick) { state.Move(tick); },
                       Simulation::RealtimePhase::Movement});
    runtime.AddSystem(
        {kMembershipSystemName, Simulation::ScheduleSlot::Hz30Realtime,
         [&state](const Simulation::TickContext &tick) { state.UpdateMembership(tick); },
         Simulation::RealtimePhase::PostPhysics});
    runtime.AddSystem({kQuerySystemName, Simulation::ScheduleSlot::Hz30Realtime,
                       [&state](const Simulation::TickContext &tick) { state.RunQuery(tick); },
                       Simulation::RealtimePhase::Queries});
}

SpatialSceneLayer::~SpatialSceneLayer() = default;

void SpatialSceneLayer::RegisterExtractors(Client::ExtractorRegistry &registry) {
    registry.Emplace<SpatialViewExtractor>(kSpatialViewExtractorName, m_State->index,
                                           m_State->volumes, m_State->query, m_State->found);
}

void SpatialSceneLayer::OnUpdate(const Application::FrameTime &time) {
    const double orbit = kTwoPi * Core::ToSeconds(time.sinceStart) / kCameraOrbitPeriodS;
    const WorldPosition eye = At(std::cos(orbit) * kCameraOrbitRadiusM, kCameraHeightM,
                                 std::sin(orbit) * kCameraOrbitRadiusM);
    m_State->camera.LookAt(eye, kCenter);
}

void SpatialSceneLayer::OnDetach() {
    const State &state = *m_State;
    const World::SpatialIndexStats index = state.index.Stats();
    const World::VolumeSetStats volumes = state.volumes.Stats();
    RUNIC_LOG_INFO("RUNIC: spatial scene: {} walkers in {} cells of {} columns ({} cell changes), "
                   "{} queries matched testing every walker ({} found, {} differed), {} walker "
                   "positions inside the cave, volumes {} ({} rule areas, {} need a Physics body, "
                   "{} indivisible)",
                   index.members, index.cells, index.columns, index.cellChanges, state.queries,
                   state.foundTotal, state.differed, state.inCave, volumes.volumes, volumes.indexed,
                   volumes.physics, volumes.indivisible);
}

} // namespace Runic::Programs
