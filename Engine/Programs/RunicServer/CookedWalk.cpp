// The server's cooked walk (CookedWalk.hpp).

#include "CookedWalk.hpp"

#include <cmath>
#include <format>
#include <string>
#include <vector>

#include "AssetCache.hpp"
#include "EntityWorld.hpp"
#include "Log.hpp"
#include "PhysicsBridge.hpp"
#include "WorldComponents.hpp"

namespace Runic::Programs {

namespace {

constexpr const char *kWalkLayerName = "engine:cooked_walk";
constexpr std::string_view kWalkSystemName = "engine:cooked_walk";
constexpr std::string_view kShapeFormat = "engine:cooked_shape_{}_{}";
constexpr std::string_view kColliderShapeFormat = "engine:cooked_collider_{}";

const char *GroundText(Physics::GroundState ground) {
    switch (ground) {
    case Physics::GroundState::OnGround:
        return "on the ground";
    case Physics::GroundState::OnSteepGround:
        return "on steep ground";
    case Physics::GroundState::NotSupported:
        return "against something it cannot stand on";
    case Physics::GroundState::InAir:
        break;
    }
    return "in the air";
}

Physics::ShapeDesc ShapeOf(const Assets::CollisionShape &shape, const Assets::CookedCollision *of) {
    switch (shape.kind) {
    case Assets::CollisionShapeKind::Sphere:
        return Physics::SphereShape(shape.radius);
    case Assets::CollisionShapeKind::Capsule:
        return Physics::CapsuleShape(shape.halfHeight, shape.radius);
    case Assets::CollisionShapeKind::Mesh:
        return Physics::MeshShape(of->meshes[shape.mesh].positions, of->meshes[shape.mesh].indices);
    case Assets::CollisionShapeKind::Box:
        break;
    }
    return Physics::BoxShape(shape.halfExtents);
}

// What the walk keeps between ticks, shared with its system.
struct WalkState {
    ECS::Entity walker;
    Core::WorldPosition origin;
    Core::WorldPosition last;
    double walkedM = 0.0;
    std::uint32_t cells = 0;
    std::uint32_t bodies = 0;
    std::string folder;
    std::string cell;
};

class CookedWalkLayer final : public Application::Layer {
  public:
    CookedWalkLayer(Simulation::SimulationRuntime &runtime, std::shared_ptr<WalkState> state)
        : Layer(kWalkLayerName), m_Runtime(runtime), m_State(std::move(state)) {}

    void OnDetach() override {
        Simulation::PhysicsBridge *bridge = m_Runtime.FindPhysicsBridge();
        const ECS::EntityWorld &entities = m_Runtime.Entities();
        const auto *character = entities.TryGet<Simulation::PhysicsCharacter>(m_State->walker);
        if (bridge == nullptr || character == nullptr) {
            RUNIC_LOG_WARNING("RUNIC: cooked walk: the character never stood in the world");
            return;
        }
        const Physics::CharacterState state = bridge->World().Character(character->handle);
        const Core::WorldDelta at = state.pose.position - m_State->origin;
        RUNIC_LOG_INFO("RUNIC: cooked walk: {} cells, {} bodies from {}; the character walked "
                       "{:.1f} m on cell {} and ends at ({:.2f}, {:.2f}, {:.2f}) from its origin, "
                       "{}",
                       m_State->cells, m_State->bodies, m_State->folder, m_State->walkedM,
                       m_State->cell, at.x, at.y, at.z, GroundText(state.ground));
    }

  private:
    Simulation::SimulationRuntime &m_Runtime;
    std::shared_ptr<WalkState> m_State;
};

} // namespace

Core::Result<std::unique_ptr<Application::Layer>>
MakeCookedWalk(Simulation::SimulationRuntime &runtime, const CookedWalkSpec &spec) {
    Assets::AssetCache cache(spec.folder);
    if (auto opened = cache.Open(); !opened) {
        return opened.GetError();
    }
    Simulation::PhysicsBridge &bridge = runtime.EnablePhysics();
    Physics::PhysicsWorld &world = bridge.World();
    ECS::EntityWorld &entities = runtime.Entities();
    auto state = std::make_shared<WalkState>();
    state->folder = spec.folder.string();
    const Physics::CollisionLayer statics = world.Layers().Find(Physics::kStaticLayerName);
    const auto place = [&](const std::string &name, const Physics::ShapeDesc &desc,
                           Core::WorldPosition at, Core::Quat rotation) -> Core::Result<void> {
        Physics::ShapeId shape = world.FindShape(name);
        if (!shape.IsValid()) {
            auto added = world.AddShape(name, desc);
            if (!added) {
                return added.GetError();
            }
            shape = added.Value();
        }
        Physics::BodyDesc body;
        body.shape = shape;
        body.layer = statics;
        const ECS::Entity entity = entities.Create();
        entities.Set(entity, World::WorldTransform{at, rotation});
        bridge.AddBody(entity, body);
        ++state->bodies;
        return {};
    };

    for (const Assets::AssetEntry &entry : cache.Catalog().Entries()) {
        if (entry.kind != Assets::AssetKind::Cell) {
            continue;
        }
        auto cell = cache.LoadCell(entry.id);
        if (!cell) {
            return cell.GetError();
        }
        if (state->cells++ == 0) {
            state->origin = cell.Value()->origin;
            state->cell = std::string(entry.name.Text());
        }
        const Core::WorldPosition origin = cell.Value()->origin;
        for (const Assets::CellPlacement &placement : cell.Value()->placements) {
            if (!placement.collision.IsValid()) {
                continue;
            }
            auto collision = cache.LoadCollision(placement.collision);
            if (!collision) {
                return collision.GetError();
            }
            const Assets::CookedCollision &shapes = collision.Value().Get();
            for (std::size_t i = 0; i < shapes.shapes.size(); ++i) {
                const Assets::CollisionShape &shape = shapes.shapes[i];
                auto placed =
                    place(std::format(kShapeFormat, Assets::AssetIdText(placement.collision), i),
                          ShapeOf(shape, &shapes),
                          origin + (placement.position + placement.rotation * shape.position),
                          placement.rotation * shape.rotation);
                if (!placed) {
                    return placed.GetError();
                }
            }
        }
        for (const Assets::CellCollider &collider : cell.Value()->colliders) {
            auto placed = place(
                std::format(kColliderShapeFormat, Assets::AssetIdText(Core::AssetId(collider.id))),
                ShapeOf(collider.shape, nullptr), origin + collider.shape.position,
                collider.shape.rotation);
            if (!placed) {
                return placed.GetError();
            }
        }
    }
    if (state->cells == 0) {
        return Core::Fail(std::format("{} holds no cell", spec.folder.string()));
    }

    Physics::CharacterDesc walker;
    walker.layer = world.Layers().Find(Physics::kCharacterLayerName);
    state->walker = entities.Create();
    state->last = state->origin + spec.spawn;
    entities.Set(state->walker, World::WorldTransform{state->last});
    bridge.AddCharacter(state->walker, walker);

    const Core::Vec3 velocity = spec.velocity;
    ECS::EntityWorld *walkers = &entities;
    runtime.AddSystem({kWalkSystemName, Simulation::ScheduleSlot::Hz30Realtime,
                       [state, velocity, walkers](const Simulation::TickContext &) {
                           auto *character =
                               walkers->TryGet<Simulation::PhysicsCharacter>(state->walker);
                           if (character == nullptr) {
                               return;
                           }
                           character->move = {velocity};
                           const Core::WorldPosition now =
                               walkers->Get<World::WorldTransform>(state->walker).position;
                           state->walkedM +=
                               std::hypot(now.x - state->last.x, now.z - state->last.z);
                           state->last = now;
                       },
                       Simulation::RealtimePhase::Commands});
    RUNIC_LOG_INFO("RUNIC: cooked walk: loaded {} cells and {} bodies from {}", state->cells,
                   state->bodies, state->folder);
    return std::unique_ptr<Application::Layer>(std::make_unique<CookedWalkLayer>(runtime, state));
}

} // namespace Runic::Programs
