#pragma once

#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "CookInput.hpp"
#include "PlainIds.hpp"

// The Cooker (COOK-1, the first cooker proof): scenes Godot imported, as plain data, into Runic's
// native formats in a cooked folder — each asset's mesh, each asset's collision (one per placement
// scale, so a runtime never scales a shape), each scene's cell of placements and colliders, and the
// folder's catalog (Runic::Assets reads them all). Collision triangles are welded and cleaned, then
// built into a Jolt shape through Runic::Physics to prove Jolt takes them. A cook either writes
// everything or, when any input is wrong, nothing: every problem is reported, naming the scene,
// the node or the asset and its source.

namespace Runic::Cooker {

// Positions closer than this weld into one vertex (they round to the same point of a 0.1 mm grid).
inline constexpr float kDefaultWeldToleranceM = 1.0e-4f;
// A collision triangle with less area is dropped (Jolt would drop it too, unreported).
inline constexpr float kMinTriangleAreaM2 = 1.0e-8f;

// The cooked folder's layout.
inline constexpr std::string_view kMeshesFolder = "Meshes";
inline constexpr std::string_view kCollisionFolder = "Collision";
inline constexpr std::string_view kCellsFolder = "Cells";

struct CookSettings {
    std::filesystem::path
        output; // the cooked folder: emptied of a previous cook's files, then filled
    float weldToleranceM = kDefaultWeldToleranceM;
};

struct CookReport {
    bool succeeded = false;
    // One sentence each: the scene and the node or asset, with its source.
    std::vector<std::string> problems;
    std::uint32_t meshes = 0;
    std::uint32_t collisions = 0;
    std::uint32_t cells = 0;
    std::uint32_t placements = 0;
    std::uint32_t colliders = 0;
    std::uint64_t collisionTrianglesIn = 0;     // before welding and cleaning
    std::uint64_t collisionTrianglesOut = 0;    // what the cooked collisions hold
    std::uint64_t weldedVertices = 0;           // vertices merged into another
    std::uint64_t droppedTriangles = 0;         // degenerate: too small, or two corners welded
    std::vector<std::filesystem::path> written; // relative to the output
    double seconds = 0.0;
};

// Cooks the scenes into settings.output. Assets several scenes share cook once.
CookReport Cook(std::span<const CookScene> scenes, const CookSettings &settings);

// The stable IDs a cook gives what has none of its own (64-bit FNV-1a of a text; never 0): a
// placement or collider from its scene and node path, a cell from its scene, an asset's collision
// from the asset and the placement's scale.
std::uint64_t PlacementIdOf(const Content::ContentId &scene, std::string_view node);
Core::AssetId CellIdOf(const Content::ContentId &scene);
Core::AssetId CollisionIdOf(Core::AssetId asset, Core::Vec3 scale);

} // namespace Runic::Cooker
