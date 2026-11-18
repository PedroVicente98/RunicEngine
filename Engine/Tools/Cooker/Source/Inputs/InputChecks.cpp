// What the Cooker checks in its input (InputChecks.hpp).

#include "Inputs/InputChecks.hpp"

#include <cmath>
#include <format>
#include <unordered_set>

#include "AssetCatalog.hpp"

namespace Runic::Cooker::Detail {

namespace {

constexpr std::size_t kTriangleCorners = 3;

bool Finite(Core::Vec3 v) noexcept {
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

bool Unit(const Core::Quat &q) noexcept {
    const float length = std::sqrt(q.w * q.w + q.x * q.x + q.y * q.y + q.z * q.z);
    return std::isfinite(length) && std::abs(length - 1.0f) <= Assets::kUnitQuaternionTolerance;
}

// What is wrong with a mesh's numbers, or nothing.
std::string MeshFault(const InputMesh &mesh) {
    if (mesh.indices.size() % kTriangleCorners != 0) {
        return std::format("{} indices, not whole triangles", mesh.indices.size());
    }
    if (!mesh.normals.empty() && mesh.normals.size() != mesh.positions.size()) {
        return std::format("{} normals for {} vertices", mesh.normals.size(),
                           mesh.positions.size());
    }
    for (const Core::Vec3 &p : mesh.positions) {
        if (!Finite(p)) {
            return "a position that is not finite";
        }
    }
    for (const std::uint32_t index : mesh.indices) {
        if (index >= mesh.positions.size()) {
            return std::format("index {} past its {} vertices", index, mesh.positions.size());
        }
    }
    return {};
}

std::size_t Triangles(const std::vector<InputMesh> &meshes) {
    std::size_t triangles = 0;
    for (const InputMesh &mesh : meshes) {
        triangles += mesh.indices.size() / kTriangleCorners;
    }
    return triangles;
}

} // namespace

void CheckAsset(const InputAsset &asset, std::vector<std::string> &problems) {
    const auto problem = [&](std::string what) {
        problems.push_back(std::format("{}: asset {} {}", asset.source, asset.name.Text(), what));
    };
    if (!asset.id.IsValid()) {
        problem("has no asset ID (Godot gives every imported file a UID)");
    }
    if (Triangles(asset.surfaces) == 0) {
        problem("has no triangles");
    }
    for (std::size_t i = 0; i < asset.surfaces.size(); ++i) {
        if (const std::string fault = MeshFault(asset.surfaces[i]); !fault.empty()) {
            problem(std::format("has a surface {} with {}", i, fault));
        }
    }
    for (std::size_t i = 0; i < asset.collisionMeshes.size(); ++i) {
        if (const std::string fault = MeshFault(asset.collisionMeshes[i]); !fault.empty()) {
            problem(std::format("has a collision mesh {} with {}", i, fault));
        }
    }
    switch (asset.collision) {
    case CollisionSource::None:
    case CollisionSource::VisualMesh:
        break;
    case CollisionSource::CollisionMeshes:
        if (Triangles(asset.collisionMeshes) == 0) {
            problem("asks for its collision meshes, and none came with it (a collision-only mesh "
                    "is named after the convention, ending in -rcol)");
        }
        break;
    case CollisionSource::ConvexHull:
        problem("asks for a convex hull collision, which the Cooker does not make yet (Physics "
                "has no convex shape): use the mesh itself or collision meshes");
        break;
    case CollisionSource::SimplifiedMesh:
        problem("asks for a simplified collision mesh, which needs meshoptimizer (not in this "
                "build yet): use the mesh itself or collision meshes");
        break;
    }
}

void CheckScene(const CookScene &scene, std::vector<std::string> &problems) {
    const auto problem = [&](std::string_view node, std::string what) {
        problems.push_back(std::format("{}: node {} {}", scene.source, node, what));
    };
    std::unordered_set<Core::AssetId> assets;
    for (const InputAsset &asset : scene.assets) {
        assets.insert(asset.id);
    }
    if (!std::isfinite(scene.origin.x) || !std::isfinite(scene.origin.y) ||
        !std::isfinite(scene.origin.z)) {
        problems.push_back(std::format("{}: the scene's origin is not finite", scene.source));
    }
    for (const InputPlacement &placement : scene.placements) {
        if (!assets.contains(placement.asset)) {
            problem(placement.node, std::format("places asset {}, which is not among the scene's "
                                                "imported assets",
                                                Assets::AssetIdText(placement.asset)));
        }
        if (!Finite(placement.position) || !Unit(placement.rotation)) {
            problem(placement.node, "has a position or a rotation that is not finite (or not a "
                                    "rotation)");
        }
        if (!Finite(placement.scale) || placement.scale.x <= 0.0f || placement.scale.y <= 0.0f ||
            placement.scale.z <= 0.0f) {
            problem(placement.node,
                    std::format("has the scale ({}, {}, {}): each part must be positive (a "
                                "mirrored placement is not cooked yet)",
                                placement.scale.x, placement.scale.y, placement.scale.z));
        }
    }
    for (const InputCollider &collider : scene.colliders) {
        const Assets::CollisionShape &shape = collider.shape;
        if (!Finite(shape.position) || !Unit(shape.rotation)) {
            problem(collider.node, "is a collider whose position or rotation is not finite");
        }
        switch (shape.kind) {
        case Assets::CollisionShapeKind::Box:
            if (!(shape.halfExtents.x > 0.0f && shape.halfExtents.y > 0.0f &&
                  shape.halfExtents.z > 0.0f)) {
                problem(collider.node, "is a box collider with a size that is not positive");
            }
            break;
        case Assets::CollisionShapeKind::Sphere:
        case Assets::CollisionShapeKind::Capsule:
            if (!(shape.radius > 0.0f) || !(shape.halfHeight >= 0.0f)) {
                problem(collider.node, "is a collider with a radius or a height that is not "
                                       "positive");
            }
            break;
        case Assets::CollisionShapeKind::Mesh:
            problem(collider.node, "is a mesh collider: a collider is a box, a sphere or a "
                                   "capsule");
            break;
        }
    }
}

} // namespace Runic::Cooker::Detail
