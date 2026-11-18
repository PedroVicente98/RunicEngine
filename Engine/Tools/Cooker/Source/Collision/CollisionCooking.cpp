// Collision cooking (CollisionCooking.hpp).

#include "Collision/CollisionCooking.hpp"

#include "Cooker.hpp"

#include <cmath>
#include <glm/geometric.hpp>
#include <limits>
#include <unordered_map>

#include "PhysicsShapes.hpp"
#include "PhysicsWorld.hpp"

namespace Runic::Cooker::Detail {

namespace {

constexpr std::size_t kTriangleCorners = 3;
constexpr std::uint32_t kUnused = std::numeric_limits<std::uint32_t>::max();
// A cross product's length is twice its triangle's area.
constexpr float kTwice = 2.0f;
const Core::Vec3 kUp(0.0f, 1.0f, 0.0f);

struct GridPoint {
    std::int64_t x;
    std::int64_t y;
    std::int64_t z;
    friend bool operator==(const GridPoint &, const GridPoint &) = default;
};

struct GridPointHash {
    std::size_t operator()(const GridPoint &p) const noexcept {
        // Large odd constants spread the three coordinates (a common spatial hash).
        constexpr std::uint64_t kX = 73856093ull;
        constexpr std::uint64_t kY = 19349663ull;
        constexpr std::uint64_t kZ = 83492791ull;
        return static_cast<std::size_t>(static_cast<std::uint64_t>(p.x) * kX ^
                                        static_cast<std::uint64_t>(p.y) * kY ^
                                        static_cast<std::uint64_t>(p.z) * kZ);
    }
};

GridPoint ToGrid(Core::Vec3 p, float tolerance) {
    return {std::llround(static_cast<double>(p.x) / tolerance),
            std::llround(static_cast<double>(p.y) / tolerance),
            std::llround(static_cast<double>(p.z) / tolerance)};
}

} // namespace

CleanedMesh CleanCollision(std::span<const InputMesh> meshes, Core::Vec3 scale,
                           float weldToleranceM) {
    CleanedMesh cleaned;
    std::unordered_map<GridPoint, std::uint32_t, GridPointHash> grid;
    std::vector<Core::Vec3> welded;
    std::vector<std::uint32_t> triangles;
    std::uint64_t vertices = 0;
    for (const InputMesh &mesh : meshes) {
        std::vector<std::uint32_t> remap(mesh.positions.size());
        for (std::size_t i = 0; i < mesh.positions.size(); ++i) {
            const Core::Vec3 p = mesh.positions[i] * scale;
            const auto [at, added] =
                grid.emplace(ToGrid(p, weldToleranceM), static_cast<std::uint32_t>(welded.size()));
            if (added) {
                welded.push_back(p);
            }
            remap[i] = at->second;
        }
        vertices += mesh.positions.size();
        cleaned.trianglesIn += mesh.indices.size() / kTriangleCorners;
        for (std::size_t t = 0; t + kTriangleCorners <= mesh.indices.size();
             t += kTriangleCorners) {
            const std::uint32_t a = remap[mesh.indices[t]];
            const std::uint32_t b = remap[mesh.indices[t + 1]];
            const std::uint32_t c = remap[mesh.indices[t + 2]];
            const float area =
                glm::length(glm::cross(welded[b] - welded[a], welded[c] - welded[a])) / kTwice;
            if (a == b || b == c || a == c || !(area >= kMinTriangleAreaM2)) {
                ++cleaned.dropped;
                continue;
            }
            triangles.insert(triangles.end(), {a, b, c});
        }
    }
    cleaned.welded = vertices - welded.size();
    // Only the vertices a triangle uses, in the order first used.
    std::vector<std::uint32_t> kept(welded.size(), kUnused);
    for (std::uint32_t &index : triangles) {
        if (kept[index] == kUnused) {
            kept[index] = static_cast<std::uint32_t>(cleaned.mesh.positions.size());
            cleaned.mesh.positions.push_back(welded[index]);
        }
        index = kept[index];
    }
    cleaned.mesh.indices = std::move(triangles);
    return cleaned;
}

Assets::CookedMesh MergeVisual(std::span<const InputMesh> surfaces) {
    Assets::CookedMesh merged;
    bool allNormals = true;
    for (const InputMesh &surface : surfaces) {
        allNormals = allNormals && surface.normals.size() == surface.positions.size();
    }
    for (const InputMesh &surface : surfaces) {
        const auto base = static_cast<std::uint32_t>(merged.positions.size());
        merged.positions.insert(merged.positions.end(), surface.positions.begin(),
                                surface.positions.end());
        if (allNormals) {
            merged.normals.insert(merged.normals.end(), surface.normals.begin(),
                                  surface.normals.end());
        }
        for (const std::uint32_t index : surface.indices) {
            merged.indices.push_back(base + index);
        }
    }
    if (!allNormals) {
        merged.normals.assign(merged.positions.size(), Core::Vec3(0.0f));
        for (std::size_t t = 0; t + kTriangleCorners <= merged.indices.size();
             t += kTriangleCorners) {
            const std::uint32_t a = merged.indices[t];
            const std::uint32_t b = merged.indices[t + 1];
            const std::uint32_t c = merged.indices[t + 2];
            // Unnormalized: longer for larger triangles, so each corner's normal is area-weighted.
            const Core::Vec3 n = glm::cross(merged.positions[b] - merged.positions[a],
                                            merged.positions[c] - merged.positions[a]);
            merged.normals[a] += n;
            merged.normals[b] += n;
            merged.normals[c] += n;
        }
        for (Core::Vec3 &n : merged.normals) {
            const float length = glm::length(n);
            n = length > 0.0f ? n / length : kUp;
        }
    }
    if (!merged.positions.empty()) {
        merged.boundsMin = merged.boundsMax = merged.positions.front();
        for (const Core::Vec3 &p : merged.positions) {
            merged.boundsMin = glm::min(merged.boundsMin, p);
            merged.boundsMax = glm::max(merged.boundsMax, p);
        }
    }
    return merged;
}

struct JoltCheck::State {
    Physics::PhysicsWorld world;
};

JoltCheck::JoltCheck() : m_State(std::make_unique<State>()) {}
JoltCheck::~JoltCheck() = default;

Core::Result<void> JoltCheck::Check(std::string_view name, const Assets::CollisionMesh &mesh) {
    auto shape = m_State->world.AddShape(name, Physics::MeshShape(mesh.positions, mesh.indices));
    if (!shape) {
        return shape.GetError();
    }
    return {};
}

} // namespace Runic::Cooker::Detail
