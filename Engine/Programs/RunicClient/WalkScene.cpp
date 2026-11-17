// RunicClient's walk scene (WalkScene.hpp).

#include "WalkScene.hpp"

#include "CommandQueue.hpp"
#include "DebugShapes.hpp"
#include "Log.hpp"
#include "PhysicsBridge.hpp"
#include "RenderData/DebugDraw.hpp"
#include "WorldComponents.hpp"

#include <glm/ext/scalar_constants.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace Runic::Programs {

namespace {

using Core::Vec3;
using Core::WorldPosition;
using RenderData::DebugDraw;

// The terrain lies 8,600 km from the origin, where a 32-bit float's step is about half a metre.
const WorldPosition kTerrainOrigin{5'000'000.0, 120.0, -7'000'000.0};

// A 64 m square of 2 m cells: flat in its middle 20 m, with rolling hills around.
constexpr int kTerrainCells = 32;
constexpr float kCellM = 2.0f;
constexpr float kTerrainHalfM = kTerrainCells * kCellM / 2.0f;
constexpr float kFlatHalfM = 10.0f;
constexpr float kHillRampM = 6.0f;
constexpr float kHillHeightM = 1.6f;
constexpr float kHillWavelengthM = 16.0f;
constexpr float kTwoPi = glm::two_pi<float>();
constexpr float kNoRise = 0.0f;
constexpr float kFullRise = 1.0f;

// On the flat middle: a 0.3 m step it climbs, a 0.6 m ledge it does not, a wall, three crates.
struct Block {
    Vec3 center; // relative to the terrain's origin
    Vec3 half;
};
constexpr std::size_t kBlockCount = 3;
const std::array<Block, kBlockCount> kBlocks{{
    {Vec3(5.0f, 0.15f, -5.0f), Vec3(2.0f, 0.15f, 2.0f)}, // the step
    {Vec3(-5.0f, 0.3f, -5.0f), Vec3(2.0f, 0.3f, 2.0f)},  // the ledge
    {Vec3(0.0f, 1.5f, -9.0f), Vec3(4.0f, 1.5f, 0.25f)},  // the wall
}};
constexpr std::size_t kCrateCount = 3;
constexpr float kCrateHalfM = 0.4f;
const std::array<Vec3, kCrateCount> kCrates{Vec3(3.0f, kCrateHalfM, 3.0f),
                                            Vec3(4.0f, kCrateHalfM, 3.0f),
                                            Vec3(3.5f, 3.0f * kCrateHalfM, 3.0f)};

// The capsule, as a physics character and as a debug shape around it.
constexpr float kCapsuleRadiusM = 0.3f;
constexpr float kCapsuleHeightM = 1.8f;
const Vec3 kCapsuleSize(2.0f * kCapsuleRadiusM, kCapsuleHeightM, 2.0f * kCapsuleRadiusM);
const Vec3 kCapsuleOffset(0.0f, kCapsuleHeightM / 2.0f, 0.0f);
const Vec3 kStart(0.0f, 0.0f, 4.0f);

// The camera follows from behind and above, looking at the capsule's chest.
const Vec3 kEyeOffset(0.0f, 4.0f, 8.0f);
const Vec3 kLookOffset(0.0f, 1.2f, 0.0f);

constexpr float kWalkMps = 4.0f;
constexpr float kRunMps = 8.0f;
constexpr float kJumpMps = 5.0f;
constexpr float kNoJumpMps = 0.0f;
// A capsule moving up faster than this after a jump was asked for has jumped.
constexpr float kRisingMps = 1.0f;

// The render IDs of what the scene draws as shapes.
constexpr std::uint64_t kCapsuleId = kCapsuleRenderId;
constexpr std::uint64_t kFirstBlockId = kCapsuleId + 1;
constexpr std::uint64_t kFirstCrateId = kFirstBlockId + kBlockCount;

// Godot's key numbering: printable keys are their upper-case code points.
constexpr std::uint32_t kKeyW = 'W';
constexpr std::uint32_t kKeyA = 'A';
constexpr std::uint32_t kKeyS = 'S';
constexpr std::uint32_t kKeyD = 'D';
constexpr std::uint32_t kKeyR = 'R';
constexpr std::uint32_t kKeySpace = ' ';

constexpr std::string_view kWalkQueueName = "engine:demo_walk";
constexpr std::string_view kPresetName = "engine:demo_keys";
constexpr std::string_view kTerrainShapeName = "engine:demo_terrain";
constexpr std::string_view kCrateShapeName = "engine:demo_crate";
constexpr std::string_view kControlsText =
    "W A S D walk  Shift+W run  Space jump  R walk on  click mark";

// Terrain colors: lower ground darker.
constexpr std::uint8_t kGroundAlpha = 255;
constexpr std::uint8_t kLowShade = 70;
constexpr std::uint8_t kHighShade = 150;
constexpr std::uint8_t kGreenLift = 40;
const RenderData::PackedColor kWireColor = RenderData::PackABGR(30, 40, 30);
// The wireframe floats a centimetre above the triangles so it is not hidden in them.
constexpr float kWireLiftM = 0.01f;
constexpr float kTextClearanceM = 0.5f;
const Vec3 kTextAbove(0.0f, kCapsuleHeightM + kTextClearanceM, 0.0f);
const Vec3 kNoOffset(0.0f);
const Core::Vec2 kStanding(0.0f);
constexpr double kMetresPerKilometre = 1000.0;
// The terrain is drawn while its middle is closer than the draw distance plus its corners' reach.
constexpr double kTerrainReachM = 1.5 * kTerrainHalfM;
constexpr std::string_view kBlockShapePrefix = "engine:demo_block_";

// The terrain's height above its origin at (x, z) metres from it.
float HeightAt(float x, float z) {
    const float out = std::max(std::abs(x), std::abs(z));
    const float rise = std::clamp((out - kFlatHalfM) / kHillRampM, kNoRise, kFullRise);
    const float waves =
        std::sin(kTwoPi * x / kHillWavelengthM) * std::sin(kTwoPi * z / kHillWavelengthM);
    return rise * kHillHeightM * (waves + kFullRise) / 2.0f;
}

struct TerrainMesh {
    std::vector<Vec3> vertices; // relative to the terrain's origin
    std::vector<std::uint32_t> indices;
};

TerrainMesh BuildTerrain() {
    TerrainMesh mesh;
    constexpr int kSide = kTerrainCells + 1;
    for (int row = 0; row < kSide; ++row) {
        for (int column = 0; column < kSide; ++column) {
            const float x = -kTerrainHalfM + static_cast<float>(column) * kCellM;
            const float z = -kTerrainHalfM + static_cast<float>(row) * kCellM;
            mesh.vertices.emplace_back(x, HeightAt(x, z), z);
        }
    }
    for (int row = 0; row < kTerrainCells; ++row) {
        for (int column = 0; column < kTerrainCells; ++column) {
            const auto at = [](int r, int c) { return static_cast<std::uint32_t>(r * kSide + c); };
            // Counter-clockwise seen from above, so each triangle faces up.
            mesh.indices.insert(mesh.indices.end(),
                                {at(row, column), at(row + 1, column), at(row, column + 1)});
            mesh.indices.insert(mesh.indices.end(), {at(row, column + 1), at(row + 1, column),
                                                     at(row + 1, column + 1)});
        }
    }
    return mesh;
}

// What the player asks for, as the simulation receives it.
struct WalkCommand {
    Vec3 velocity{0.0f};
    float jumpSpeed = 0.0f;
};

const char *GroundName(Physics::GroundState ground) {
    switch (ground) {
    case Physics::GroundState::OnGround:
        return "on the ground";
    case Physics::GroundState::OnSteepGround:
        return "on steep ground";
    case Physics::GroundState::NotSupported:
        return "against a wall";
    case Physics::GroundState::InAir:
        return "in the air";
    }
    return "in the air";
}

// Draws the terrain (shaded triangles and their wireframe) and the controls above the capsule.
class TerrainExtractor final : public Client::Extractor {
  public:
    TerrainExtractor(const TerrainMesh &mesh, const ECS::EntityWorld &world, ECS::Entity capsule)
        : m_Mesh(mesh), m_World(world), m_Capsule(capsule) {}

    void Extract(const Client::ExtractionView &view, RenderData::RenderFrame &frame) override {
        const double reach = view.drawDistanceM + kTerrainReachM;
        if (Core::DistanceSq(kTerrainOrigin, view.origin) > reach * reach) {
            return; // too far to see, and too far for a float
        }
        const Vec3 shift = Core::ToLocal(kTerrainOrigin - view.origin);
        DebugDraw depth(frame.debug);
        for (std::size_t i = 0; i < m_Mesh.indices.size(); i += kTriangleCorners) {
            const Vec3 a = m_Mesh.vertices[m_Mesh.indices[i]];
            const Vec3 b = m_Mesh.vertices[m_Mesh.indices[i + 1]];
            const Vec3 c = m_Mesh.vertices[m_Mesh.indices[i + 2]];
            const float height = (a.y + b.y + c.y) / static_cast<float>(kTriangleCorners);
            const auto shade = static_cast<std::uint8_t>(
                kLowShade +
                (kHighShade - kLowShade) * std::clamp(height / kHillHeightM, kNoRise, kFullRise));
            const RenderData::PackedColor color = RenderData::PackABGR(
                shade, static_cast<std::uint8_t>(shade + kGreenLift), shade, kGroundAlpha);
            depth.FillTriangle(shift + a, shift + b, shift + c, color);
            const Vec3 lift(0.0f, kWireLiftM, 0.0f);
            depth.DrawLine(shift + a + lift, shift + b + lift, kWireColor);
            depth.DrawLine(shift + b + lift, shift + c + lift, kWireColor);
        }
        if (const auto *at = m_World.TryGet<World::WorldTransform>(m_Capsule);
            at != nullptr && Core::DistanceSq(at->position, view.origin) < reach * reach) {
            DebugDraw overlay(frame.debug, RenderData::DebugBatch::Overlay);
            overlay.DrawText(Core::ToLocal(at->position - view.origin) + kTextAbove, kControlsText,
                             RenderData::kDebugText);
        }
    }

  private:
    static constexpr std::size_t kTriangleCorners = 3;

    const TerrainMesh &m_Mesh;
    const ECS::EntityWorld &m_World;
    ECS::Entity m_Capsule;
};

// The simulation's physics near the camera, into the frame's physics geometry (PHYS-3's debug view,
// drawn by GDB-4's layer). It is the physics geometry's one writer: it empties it first.
class PhysicsGeometryExtractor final : public Client::Extractor {
  public:
    PhysicsGeometryExtractor(Simulation::PhysicsBridge &physics,
                             RenderData::DebugMeshLibrary &meshes)
        : m_Physics(physics), m_Meshes(meshes) {
        m_Settings.radiusM = kPhysicsViewRadiusM;
    }

    void Extract(const Client::ExtractionView &view, RenderData::RenderFrame &frame) override {
        frame.physics.Clear();
        m_Physics.World().ExtractDebugGeometry(view.origin, m_Settings, frame.physics, &m_Meshes);
    }

  private:
    static constexpr double kPhysicsViewRadiusM = 100.0;

    Simulation::PhysicsBridge &m_Physics;
    RenderData::DebugMeshLibrary &m_Meshes;
    Physics::PhysicsDebugSettings m_Settings;
};

} // namespace

struct WalkSceneLayer::State {
    State(Simulation::SimulationRuntime &simulation, Client::ClientCamera &view,
          Client::InputLayer &inputLayer)
        : runtime(simulation), camera(view), input(inputLayer), walks(simulation, kWalkQueueName) {}

    Simulation::SimulationRuntime &runtime;
    Client::ClientCamera &camera;
    Client::InputLayer &input;
    Simulation::CommandQueue<WalkCommand> walks;
    TerrainMesh terrain;
    ECS::Entity capsule;

    // The actions.
    Client::MovementActions move;
    Client::ActionId run;
    Client::ActionId jump;
    Client::ActionId autoWalk;
    Client::ActionId mark;
    bool walkingOnItsOwn = false;

    // What the Commands phase applies each tick: the latest velocity asked for, and a jump.
    Vec3 velocity{0.0f};
    float jumpSpeed = 0.0f;
    bool jumpAsked = false;

    // What the capsule did.
    double walkedM = 0.0;
    double highestM = 0.0;
    std::uint32_t jumps = 0;
    std::uint32_t marks = 0;
    WorldPosition last;
};

WalkSceneLayer::WalkSceneLayer(Simulation::SimulationRuntime &runtime, Client::ClientCamera &camera,
                               Client::InputLayer &input)
    : Layer(std::string(kWalkSceneLayerName)),
      m_State(std::make_unique<State>(runtime, camera, input)) {
    State &state = *m_State;
    ECS::EntityWorld &world = runtime.Entities();
    Client::RegisterClientComponents(world);
    Simulation::PhysicsBridge &physics = runtime.EnablePhysics();
    Physics::PhysicsWorld &jolt = physics.World();
    const Physics::CollisionLayer still = jolt.Layers().Find(Physics::kStaticLayerName);
    const auto at = [](Vec3 offset) {
        return WorldPosition{kTerrainOrigin.x + offset.x, kTerrainOrigin.y + offset.y,
                             kTerrainOrigin.z + offset.z};
    };

    // The terrain: one static mesh.
    state.terrain = BuildTerrain();
    Physics::BodyDesc ground;
    ground.shape = jolt.AddShape(kTerrainShapeName,
                                 Physics::MeshShape(state.terrain.vertices, state.terrain.indices))
                       .Value();
    ground.layer = still;
    const ECS::Entity terrain = world.Create();
    world.Set(terrain, World::WorldTransform{kTerrainOrigin, World::kNoRotation});
    physics.AddBody(terrain, ground);

    // The blocks and the crates, drawn as shapes.
    for (std::size_t i = 0; i < kBlockCount; ++i) {
        const Block &block = kBlocks[i];
        Physics::BodyDesc desc;
        desc.shape = jolt.AddShape(std::string(kBlockShapePrefix) + std::to_string(i),
                                   Physics::BoxShape(block.half))
                         .Value();
        desc.layer = still;
        const ECS::Entity entity = world.Create();
        world.Set(entity, World::WorldTransform{at(block.center), World::kNoRotation});
        world.Set(entity, Client::DebugShape{RenderData::ShapeKind::Box, block.half * 2.0f,
                                             kNoOffset, RenderData::kDebugInactive});
        world.Set(entity, Client::RenderIdentity{Core::RenderInstanceId(kFirstBlockId + i)});
        physics.AddBody(entity, desc);
    }
    Physics::BodyDesc crate;
    crate.shape = jolt.AddShape(kCrateShapeName, Physics::BoxShape(Vec3(kCrateHalfM))).Value();
    crate.motion = Physics::MotionType::Dynamic;
    crate.layer = jolt.Layers().Find(Physics::kMovingLayerName);
    for (std::size_t i = 0; i < kCrateCount; ++i) {
        const ECS::Entity entity = world.Create();
        world.Set(entity, World::WorldTransform{at(kCrates[i]), World::kNoRotation});
        world.Set(entity, Client::DebugShape{RenderData::ShapeKind::Box, Vec3(2.0f * kCrateHalfM),
                                             kNoOffset, RenderData::kDebugPhysics});
        world.Set(entity, Client::RenderIdentity{Core::RenderInstanceId(kFirstCrateId + i)});
        physics.AddBody(entity, crate);
    }

    // The capsule, followed by the camera.
    state.capsule = world.Create();
    state.last = at(kStart);
    world.Set(state.capsule, World::WorldTransform{state.last, World::kNoRotation});
    world.Set(state.capsule, Client::DebugShape{RenderData::ShapeKind::Capsule, kCapsuleSize,
                                                kCapsuleOffset, RenderData::kDebugPredicted});
    world.Set(state.capsule, Client::RenderIdentity{Core::RenderInstanceId(kCapsuleId)});
    Physics::CharacterDesc character;
    character.radius = kCapsuleRadiusM;
    character.height = kCapsuleHeightM;
    character.layer = jolt.Layers().Find(Physics::kCharacterLayerName);
    physics.AddCharacter(state.capsule, character);
    camera.Follow(state.capsule, kEyeOffset, kLookOffset);

    // The actions and the keys: the scene's own preset.
    Client::InputMap &map = input.Map();
    state.move.forward = map.AddAction("engine:demo_forward").Value();
    state.move.back = map.AddAction("engine:demo_back").Value();
    state.move.left = map.AddAction("engine:demo_left").Value();
    state.move.right = map.AddAction("engine:demo_right").Value();
    state.run = map.AddAction("engine:demo_run").Value();
    state.jump = map.AddAction("engine:demo_jump").Value();
    state.autoWalk = map.AddAction("engine:demo_walk_on").Value();
    state.mark = map.AddAction("engine:demo_mark").Value();
    using Client::Bind;
    using Client::KeyInput;
    Client::InputPreset keys{
        std::string(kPresetName),
        {Bind(state.move.forward, KeyInput(kKeyW)), Bind(state.move.back, KeyInput(kKeyS)),
         Bind(state.move.left, KeyInput(kKeyA)), Bind(state.move.right, KeyInput(kKeyD)),
         Bind(state.run, KeyInput(kKeyW), Application::kModifierShift),
         Bind(state.jump, KeyInput(kKeySpace)), Bind(state.autoWalk, KeyInput(kKeyR)),
         Bind(state.mark, Client::MouseInput(Application::MouseButton::Left))}};
    if (Core::Result<void> added = map.AddPreset(std::move(keys)); !added) {
        RUNIC_LOG_ERROR("RUNIC: walk scene: {}", added.GetError().message);
    }
    if (Core::Result<void> used = map.UsePreset(kPresetName); !used) {
        RUNIC_LOG_ERROR("RUNIC: walk scene: {}", used.GetError().message);
    }

    // The command builder: the actions of this frame become one walk command.
    State *scene = &state;
    input.AddCommandBuilder("engine:demo_walk", [scene](const Client::InputMap &actions,
                                                        const Application::FrameTime &time) {
        if (actions.WasPressed(scene->autoWalk)) {
            scene->walkingOnItsOwn = !scene->walkingOnItsOwn;
        }
        if (actions.WasPressed(scene->mark)) {
            ++scene->marks;
            RUNIC_LOG_INFO("RUNIC: action engine:demo_mark pressed at frame {}", time.frame);
        }
        const bool running = actions.IsHeld(scene->run);
        Core::Vec2 direction = Client::MovementDirection(actions, scene->move);
        if (running || (scene->walkingOnItsOwn && direction == kStanding)) {
            direction.y = kFullRise; // Shift+W runs forward; walking on its own goes forward
        }
        const Vec3 heading =
            Client::WorldMovement(direction, scene->camera.Eye(), scene->camera.Target());
        WalkCommand walk;
        walk.velocity = heading * (running ? kRunMps : kWalkMps);
        walk.jumpSpeed = actions.WasPressed(scene->jump) ? kJumpMps : kNoJumpMps;
        scene->walks.Push(walk);
    });

    // The Commands phase: the latest velocity holds until another arrives; a jump is asked once.
    ECS::EntityWorld *entities = &world;
    runtime.AddSystem({"engine:demo_apply_walk", Simulation::ScheduleSlot::Hz30Realtime,
                       [scene, entities](const Simulation::TickContext &) {
                           scene->walks.Drain([scene](const WalkCommand &walk) {
                               scene->velocity = walk.velocity;
                               scene->jumpSpeed = std::max(scene->jumpSpeed, walk.jumpSpeed);
                           });
                           if (auto *character =
                                   entities->TryGet<Simulation::PhysicsCharacter>(scene->capsule)) {
                               character->move = {scene->velocity, scene->jumpSpeed};
                               scene->jumpAsked = scene->jumpAsked || scene->jumpSpeed > kNoJumpMps;
                               scene->jumpSpeed = kNoJumpMps;
                           }
                       },
                       Simulation::RealtimePhase::Commands});
    // After physics: what the capsule did.
    Simulation::PhysicsBridge *bridge = &physics;
    runtime.AddSystem(
        {"engine:demo_walk_record", Simulation::ScheduleSlot::Hz30Realtime,
         [scene, entities, bridge](const Simulation::TickContext &) {
             const auto *character = entities->TryGet<Simulation::PhysicsCharacter>(scene->capsule);
             if (character == nullptr) {
                 return;
             }
             const WorldPosition now =
                 entities->Get<World::WorldTransform>(scene->capsule).position;
             scene->walkedM += std::hypot(now.x - scene->last.x, now.z - scene->last.z);
             scene->highestM = std::max(scene->highestM, now.y - kTerrainOrigin.y);
             scene->last = now;
             if (scene->jumpAsked) {
                 const Physics::CharacterState state = bridge->World().Character(character->handle);
                 if (state.ground == Physics::GroundState::InAir && state.velocity.y > kRisingMps) {
                     ++scene->jumps;
                     scene->jumpAsked = false;
                 } else if (state.ground == Physics::GroundState::OnGround) {
                     scene->jumpAsked = false; // it could not jump (it was not settled)
                 }
             }
         },
         Simulation::RealtimePhase::Queries});
}

WalkSceneLayer::~WalkSceneLayer() = default;

void WalkSceneLayer::RegisterExtractors(Client::ExtractorRegistry &registry,
                                        RenderData::DebugMeshLibrary &meshes) {
    registry.Emplace<TerrainExtractor>(kTerrainExtractorName, m_State->terrain,
                                       m_State->runtime.Entities(), m_State->capsule);
    registry.Emplace<PhysicsGeometryExtractor>(kPhysicsGeometryExtractorName,
                                               *m_State->runtime.FindPhysicsBridge(), meshes);
}

void WalkSceneLayer::OnDetach() {
    const State &state = *m_State;
    const char *ground = "in the air";
    Simulation::PhysicsBridge *physics = state.runtime.FindPhysicsBridge();
    if (const auto *character =
            state.runtime.Entities().TryGet<Simulation::PhysicsCharacter>(state.capsule);
        character != nullptr && physics != nullptr) {
        ground = GroundName(physics->World().Character(character->handle).ground);
    }
    RUNIC_LOG_INFO("RUNIC: walk scene: the capsule walked {:.1f} m on terrain {:.0f} km from the "
                   "origin, jumps {}, marks {}, highest {:.2f} m, ends {}",
                   state.walkedM,
                   std::hypot(kTerrainOrigin.x, kTerrainOrigin.z) / kMetresPerKilometre,
                   state.jumps, state.marks, state.highestM, ground);
}

} // namespace Runic::Programs
