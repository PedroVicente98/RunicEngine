// The cook (Cooker.hpp): check everything, build every file in memory, then write them all.

#include "Cooker.hpp"

#include <chrono>
#include <cmath>
#include <cstring>
#include <format>
#include <map>

#include "AssetCatalog.hpp"
#include "Collision/CollisionCooking.hpp"
#include "CookedFormats.hpp"
#include "Inputs/InputChecks.hpp"

namespace Runic::Cooker {

namespace {

// Scales equal to a thousandth share one collision, cooked at that thousandth.
constexpr float kScaleSteps = 1000.0f;
constexpr std::int64_t kUnitScaleSteps = 1000;
constexpr std::string_view kCollisionSuffix = "collision";
// Slice 9 cooks a scene into one cell; 64 m cells and their names are COOK-3's.
constexpr std::string_view kOnlyCellSuffix = "cell_0_0_0";
constexpr std::uint64_t kNeverZero = 1;

std::uint64_t NonZero(std::uint64_t hash) noexcept { return hash == 0 ? kNeverZero : hash; }

struct ScaleSteps {
    std::int64_t x;
    std::int64_t y;
    std::int64_t z;
    bool IsUnit() const noexcept {
        return x == kUnitScaleSteps && y == kUnitScaleSteps && z == kUnitScaleSteps;
    }
    Core::Vec3 Scale() const noexcept {
        return {static_cast<float>(x) / kScaleSteps, static_cast<float>(y) / kScaleSteps,
                static_cast<float>(z) / kScaleSteps};
    }
};

ScaleSteps StepsOf(Core::Vec3 scale) noexcept {
    return {std::llround(scale.x * kScaleSteps), std::llround(scale.y * kScaleSteps),
            std::llround(scale.z * kScaleSteps)};
}

std::string CollisionName(const Content::ContentId &asset, ScaleSteps steps) {
    if (steps.IsUnit()) {
        return std::format("{}/{}", asset.Text(), kCollisionSuffix);
    }
    return std::format("{}/{}_{}_{}_{}", asset.Text(), kCollisionSuffix, steps.x, steps.y, steps.z);
}

std::string FileOf(std::string_view folder, Core::AssetId id, std::string_view extension) {
    return std::format("{}/{}{}", folder, Assets::AssetIdText(id), extension);
}

struct PendingFile {
    std::string relative;
    std::vector<std::byte> bytes;
};

} // namespace

std::uint64_t PlacementIdOf(const Content::ContentId &scene, std::string_view node) {
    return NonZero(Content::HashText(std::format("{}/{}", scene.Text(), node)));
}

Core::AssetId CellIdOf(const Content::ContentId &scene) {
    return Core::AssetId(
        NonZero(Content::HashText(std::format("{}/{}", scene.Text(), kOnlyCellSuffix))));
}

Core::AssetId CollisionIdOf(Core::AssetId asset, Core::Vec3 scale) {
    const ScaleSteps steps = StepsOf(scale);
    return Core::AssetId(
        NonZero(Content::HashText(std::format("{}/{}/{}/{}/{}", Assets::AssetIdText(asset),
                                              kCollisionSuffix, steps.x, steps.y, steps.z))));
}

CookReport Cook(std::span<const CookScene> scenes, const CookSettings &settings) {
    const auto started = std::chrono::steady_clock::now();
    CookReport report;
    const auto finish = [&]() {
        report.succeeded = report.problems.empty();
        report.seconds =
            std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
        return report;
    };

    // 1. Everything read is checked; an asset several scenes share is checked once.
    std::map<Core::AssetId, const InputAsset *> assets;
    for (const CookScene &scene : scenes) {
        for (const InputAsset &asset : scene.assets) {
            const auto [known, added] = assets.emplace(asset.id, &asset);
            if (added) {
                Detail::CheckAsset(asset, report.problems);
            } else if (known->second->name != asset.name || known->second->source != asset.source) {
                report.problems.push_back(
                    std::format("{}: asset {} has the ID {} of asset {} ({})", asset.source,
                                asset.name.Text(), Assets::AssetIdText(asset.id),
                                known->second->name.Text(), known->second->source));
            }
        }
        Detail::CheckScene(scene, report.problems);
    }
    if (!report.problems.empty()) {
        return finish();
    }

    // 2. Every file in memory, and the catalog.
    std::vector<PendingFile> files;
    Assets::AssetCatalog catalog;
    const auto add = [&](Core::AssetId id, Assets::AssetKind kind, const std::string &name,
                         std::string relative, std::vector<std::byte> bytes,
                         const std::string &source) {
        auto parsed = Content::ContentId::Parse(name);
        if (!parsed) {
            report.problems.push_back(std::format("{}: cannot name what it cooks into: {}", source,
                                                  parsed.GetError().message));
            return;
        }
        if (!catalog.Add({id, kind, parsed.Value(), relative}, Content::FromFile(source))) {
            report.problems.push_back(catalog.Problems().back().message);
            return;
        }
        files.push_back({std::move(relative), std::move(bytes)});
    };

    for (const auto &[id, asset] : assets) {
        add(id, Assets::AssetKind::Mesh, std::string(asset->name.Text()),
            FileOf(kMeshesFolder, id, Assets::kMeshExtension),
            Assets::WriteCookedMesh(Detail::MergeVisual(asset->surfaces)), asset->source);
        ++report.meshes;
    }

    Detail::JoltCheck jolt;
    std::map<Core::AssetId, bool> collisions; // cooked, and whether it has triangles
    for (const CookScene &scene : scenes) {
        Assets::CookedCell cell;
        cell.origin = scene.origin;
        for (const InputPlacement &placement : scene.placements) {
            const InputAsset &asset = *assets.at(placement.asset);
            Assets::CellPlacement placed;
            placed.id = PlacementIdOf(scene.name, placement.node);
            placed.mesh = asset.id;
            placed.position = placement.position;
            placed.rotation = placement.rotation;
            placed.scale = placement.scale;
            if (asset.collision != CollisionSource::None) {
                const ScaleSteps steps = StepsOf(placement.scale);
                const Core::AssetId collisionId = CollisionIdOf(asset.id, placement.scale);
                if (!collisions.contains(collisionId)) {
                    const auto &meshes = asset.collision == CollisionSource::CollisionMeshes
                                             ? asset.collisionMeshes
                                             : asset.surfaces;
                    Detail::CleanedMesh cleaned =
                        Detail::CleanCollision(meshes, steps.Scale(), settings.weldToleranceM);
                    report.collisionTrianglesIn += cleaned.trianglesIn;
                    report.weldedVertices += cleaned.welded;
                    report.droppedTriangles += cleaned.dropped;
                    const std::string name = CollisionName(asset.name, steps);
                    const bool hasTriangles = !cleaned.mesh.indices.empty();
                    collisions.emplace(collisionId, hasTriangles);
                    if (!hasTriangles) {
                        report.problems.push_back(std::format(
                            "{}: asset {} has no collision triangles left once cleaned (every one "
                            "smaller than {} m²)",
                            asset.source, asset.name.Text(), kMinTriangleAreaM2));
                        continue;
                    }
                    if (auto built = jolt.Check(name, cleaned.mesh); !built) {
                        report.problems.push_back(
                            std::format("{}: Jolt does not take asset {}'s collision: {}",
                                        asset.source, asset.name.Text(), built.GetError().message));
                        continue;
                    }
                    report.collisionTrianglesOut += cleaned.mesh.indices.size() / 3;
                    Assets::CookedCollision collision;
                    Assets::CollisionShape shape;
                    shape.kind = Assets::CollisionShapeKind::Mesh;
                    collision.meshes.push_back(std::move(cleaned.mesh));
                    collision.shapes.push_back(shape);
                    add(collisionId, Assets::AssetKind::Collision, name,
                        FileOf(kCollisionFolder, collisionId, Assets::kCollisionExtension),
                        Assets::WriteCookedCollision(collision), asset.source);
                    ++report.collisions;
                }
                if (collisions.at(collisionId)) {
                    placed.collision = collisionId;
                }
            }
            cell.placements.push_back(placed);
            ++report.placements;
        }
        for (const InputCollider &collider : scene.colliders) {
            cell.colliders.push_back({PlacementIdOf(scene.name, collider.node), collider.shape});
            ++report.colliders;
        }
        const Core::AssetId cellId = CellIdOf(scene.name);
        add(cellId, Assets::AssetKind::Cell,
            std::format("{}/{}", scene.name.Text(), kOnlyCellSuffix),
            FileOf(kCellsFolder, cellId, Assets::kCellExtension), Assets::WriteCookedCell(cell),
            scene.source);
        ++report.cells;
    }
    if (!report.problems.empty()) {
        return finish();
    }

    // 3. A full cook replaces the previous one: its folders go, then every file, the catalog last.
    std::error_code error;
    for (const std::string_view folder : {kMeshesFolder, kCollisionFolder, kCellsFolder}) {
        std::filesystem::remove_all(settings.output / folder, error);
    }
    std::filesystem::remove(settings.output / Assets::kCatalogFileName, error);
    const std::string text = catalog.Write();
    std::vector<std::byte> catalogBytes(text.size());
    std::memcpy(catalogBytes.data(), text.data(), text.size());
    files.push_back({std::string(Assets::kCatalogFileName), std::move(catalogBytes)});
    for (const PendingFile &file : files) {
        if (auto written = Assets::WriteBytesToFile(settings.output / file.relative, file.bytes);
            !written) {
            report.problems.push_back(written.GetError().message);
            return finish();
        }
        report.written.emplace_back(file.relative);
    }
    return finish();
}

} // namespace Runic::Cooker
