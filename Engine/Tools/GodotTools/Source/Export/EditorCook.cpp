// Cooking the project's scenes, and loading the result into the editor's local world
// (EditorCook.hpp).

#include "EditorCook.hpp"

#include <format>
#include <string>

#include "AssetCache.hpp"
#include "ContentId.hpp"
#include "EntityWorld.hpp"
#include "PhysicsBridge.hpp"
#include "SimulationRuntime.hpp"
#include "WorldComponents.hpp"

namespace Runic::GodotTools {

namespace {

// The entities LoadCookedCells made: the next load replaces them.
struct CookedBody {
    std::uint64_t placement = 0;
};

constexpr std::string_view kCookedBodyName = "engine:cooked_body";
constexpr std::string_view kCookedBodyDescription =
    "A static body the editor loaded from the cooked folder (the placement or collider's ID)";
constexpr std::string_view kCookedShapeFormat = "engine:cooked_shape_{:016x}";

void FormatCookedBody(const void *component, std::string &out) {
    out = Assets::AssetIdText(Core::AssetId(static_cast<const CookedBody *>(component)->placement));
}

// A shape's name in the physics world: its content's hash, so a reload of the same cook reuses it
// and an edited one (the same IDs, new triangles) gets its own.
std::uint64_t ContentHashOf(const Assets::CollisionShape &shape,
                            const Assets::CookedCollision *of) {
    std::string bytes(reinterpret_cast<const char *>(&shape.kind), sizeof(shape.kind));
    const auto append = [&](const void *data, std::size_t size) {
        bytes.append(static_cast<const char *>(data), size);
    };
    append(&shape.halfExtents, sizeof(shape.halfExtents));
    append(&shape.radius, sizeof(shape.radius));
    append(&shape.halfHeight, sizeof(shape.halfHeight));
    if (shape.kind == Assets::CollisionShapeKind::Mesh && of != nullptr) {
        const Assets::CollisionMesh &mesh = of->meshes[shape.mesh];
        append(mesh.positions.data(), mesh.positions.size() * sizeof(Core::Vec3));
        append(mesh.indices.data(), mesh.indices.size() * sizeof(std::uint32_t));
    }
    return Content::HashText(bytes);
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

} // namespace

Core::Result<Cooker::CookReport> CookEditorScenes(const EditorCookSpec &spec) {
    std::vector<Cooker::CookScene> scenes;
    for (const std::string &scene : spec.scenes) {
        auto read = ReadImportedScene(scene, spec.origin, spec.contentFolder);
        if (!read) {
            return read.GetError();
        }
        scenes.push_back(std::move(read.Value()));
    }
    Cooker::CookSettings settings;
    settings.output = spec.output;
    return Cooker::Cook(scenes, settings);
}

Core::Result<CookedLoadStats> LoadCookedCells(Simulation::SimulationRuntime &runtime,
                                              const std::filesystem::path &cooked) {
    Simulation::PhysicsBridge *bridge = runtime.FindPhysicsBridge();
    if (bridge == nullptr) {
        return Core::Fail("the local simulation runs no physics: the cook is not loaded into it");
    }
    Assets::AssetCache cache(cooked);
    if (auto opened = cache.Open(); !opened) {
        return opened.GetError();
    }
    ECS::EntityWorld &entities = runtime.Entities();
    if (!entities.IsRegistered<CookedBody>()) {
        ECS::ComponentOptions options;
        options.description = kCookedBodyDescription;
        options.format = &FormatCookedBody;
        entities.RegisterComponent<CookedBody>(kCookedBodyName, options);
    }
    CookedLoadStats stats;
    std::vector<ECS::Entity> previous;
    entities.ForEachEntity([&](ECS::Entity entity) {
        if (entities.Has<CookedBody>(entity)) {
            previous.push_back(entity);
        }
    });
    for (const ECS::Entity entity : previous) {
        entities.Destroy(entity); // the bridge takes its body at the next barrier
    }
    stats.removed = static_cast<std::uint32_t>(previous.size());

    Physics::PhysicsWorld &world = bridge->World();
    const Physics::CollisionLayer statics = world.Layers().Find(Physics::kStaticLayerName);
    const auto place = [&](std::uint64_t id, const std::string &shapeName,
                           const Physics::ShapeDesc &desc, Core::WorldPosition at,
                           Core::Quat rotation) -> Core::Result<void> {
        Physics::ShapeId shape = world.FindShape(shapeName);
        if (!shape.IsValid()) {
            auto added = world.AddShape(shapeName, desc);
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
        entities.Set(entity, CookedBody{id});
        bridge->AddBody(entity, body);
        ++stats.bodies;
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
        ++stats.cells;
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
            for (const Assets::CollisionShape &shape : shapes.shapes) {
                const std::string name =
                    std::format(kCookedShapeFormat, ContentHashOf(shape, &shapes));
                auto placed =
                    place(placement.id, name, ShapeOf(shape, &shapes),
                          origin + (placement.position + placement.rotation * shape.position),
                          placement.rotation * shape.rotation);
                if (!placed) {
                    return placed.GetError();
                }
            }
        }
        for (const Assets::CellCollider &collider : cell.Value()->colliders) {
            const std::string name =
                std::format(kCookedShapeFormat, ContentHashOf(collider.shape, nullptr));
            auto placed = place(collider.id, name, ShapeOf(collider.shape, nullptr),
                                origin + collider.shape.position, collider.shape.rotation);
            if (!placed) {
                return placed.GetError();
            }
        }
    }
    return stats;
}

} // namespace Runic::GodotTools
