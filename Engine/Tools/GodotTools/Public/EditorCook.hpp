#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include "CookInput.hpp"
#include "Cooker.hpp"
#include "Result.hpp"
#include "WorldSpace.hpp"

namespace Runic::Simulation {
class SimulationRuntime;
}

// Cooker integration (GDT-4; RunicDocs › World authoring and cooking: Godot and Blender, How
// collision is marked, Cooker). Godot imports the glTF; the RunicEditor reads the scenes it
// imported and hands them to the Runic Cooker as plain data (Cooker::CookScene), so there is no
// second glTF importer. What the reader takes from a scene:
//
// - Each MeshInstance3D is an asset (the imported file's UID and the node's path in it: the
// Cooker's
//   AssetId) and a placement (the node's path and pose in the cooked scene). Godot's triangles are
//   clockwise; they are turned counter-clockwise, as the Runic formats store them.
// - The import settings' collision preset (the Import dock's "Runic › Collision" for the file:
//   the mesh itself, none, collision meshes only) is put on each mesh node at import.
// - The naming convention: a mesh node named "<Name>-rcol" (or "<Name>-rcol<anything>", for
//   several pieces) is collision only, for the mesh node <Name> that is its parent or its sibling:
//   never drawn, cooked into <Name>'s collision, which then uses only such meshes, whatever the
//   file's preset. It is how an artist exports a simple collision from Blender beside a detailed
//   visual.
// - A RunicCollider3D is a collider: a box, sphere or capsule placed in the map.
//
// Everything here runs on the editor's main thread with Godot running (it loads imported
// resources).

namespace Runic::GodotTools {

// The naming convention's suffix, and the names the import plugin and the reader share.
inline constexpr std::string_view kCollisionOnlySuffix = "-rcol";
inline constexpr std::string_view kCollisionPresetOption = "runic/collision";
inline constexpr std::string_view kCollisionPresetMeta = "runic_collision";
inline constexpr std::string_view kCollisionOnlyMeta = "runic_collision_only";

// The presets the Import dock offers, in its order; the reader maps them to Cooker sources.
enum class CollisionPreset : std::int64_t {
    MeshItself = 0,
    None = 1,
    CollisionMeshes = 2,
    ConvexHull = 3,
    SimplifiedMesh = 4,
};
// "The mesh itself,None,Collision meshes (-rcol),Convex hull (not cooked yet),Simplified mesh (not
// cooked yet)": the Import dock's list.
std::string_view CollisionPresetHint() noexcept;

// The project's content folder when the manifest does not say (ProjectPaths::content).
inline constexpr std::string_view kDefaultContentFolder = "Content";

// What a cook of the project's scenes takes.
struct EditorCookSpec {
    std::vector<std::string> scenes; // res:// paths
    std::filesystem::path output;    // the cooked folder (the manifest's paths.cooked)
    // The folder content names start under (the manifest's paths.content).
    std::string contentFolder = std::string(kDefaultContentFolder);
    // The world position of the scenes' origin: EditorToolsSpec::editedSceneOrigin until region
    // tools give each region its own.
    Core::WorldPosition origin;
};

// A scene Godot imported, read as the Cooker's input: an error when it cannot be loaded, or when a
// collision-only mesh has no owner.
Core::Result<Cooker::CookScene>
ReadImportedScene(std::string_view scene, Core::WorldPosition origin,
                  std::string_view contentFolder = kDefaultContentFolder);

// Reads every scene and cooks them into the output: the Cooker's report (an error when a scene
// cannot be read).
Core::Result<Cooker::CookReport> CookEditorScenes(const EditorCookSpec &spec);

// What LoadCookedCells put into a simulation.
struct CookedLoadStats {
    std::uint32_t cells = 0;
    std::uint32_t bodies = 0;  // static bodies: placements' collision shapes and colliders
    std::uint32_t removed = 0; // bodies a previous load put there and this one replaced
};

// Puts a cooked folder's cells into a local simulation's physics, as a server will (static bodies
// at each cell's origin plus their offsets), replacing what an earlier call put there: the editor
// loads its cook so the "see what the server sees" layer draws the cooked collision over the
// visuals. Between steps, on the simulation's thread; the simulation runs physics.
Core::Result<CookedLoadStats> LoadCookedCells(Simulation::SimulationRuntime &runtime,
                                              const std::filesystem::path &cooked);

// The content name of what a project file holds: "game:" and its path under the content folder,
// without its extension, in snake case (res://Content/TestRegion/test_region.glb and its node
// Platform: game:test_region/test_region/platform).
std::string ContentNameOf(std::string_view resPath, std::string_view nodePath,
                          std::string_view contentFolder);

} // namespace Runic::GodotTools
