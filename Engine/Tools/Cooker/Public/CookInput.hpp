#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "ContentId.hpp"
#include "CookedFormats.hpp"
#include "Math.hpp"
#include "PlainIds.hpp"
#include "WorldSpace.hpp"

// What the Cooker reads (COOK-1; RunicDocs › World authoring and cooking: "the Cooker reads
// geometry from the scenes Godot imported instead of running a second importer"). GodotTools' cook
// reads a scene Godot imported — its meshes, the import settings' collision presets, the collision
// meshes named by the convention, the RunicCollider3D markers — and hands it over as this plain
// data: the Cooker links no Godot and parses no glTF. Positions are metres, y up, as the editor
// saved them: assets in their own space, placements relative to the scene's origin, which is a
// world position in 64 bits.

namespace Runic::Cooker {

// Where an asset's collision comes from: its import preset (RunicDocs › World authoring and cooking
// › How collision is marked).
enum class CollisionSource : std::uint8_t {
    None,            // no collision: grass, a banner
    VisualMesh,      // the mesh itself: rocks and buildings modelled to be walked on
    CollisionMeshes, // the collision-only meshes that came with it, under the naming convention
    ConvexHull,      // not cooked yet: Physics has no convex shape (reported)
    SimplifiedMesh,  // not cooked yet: simplification needs meshoptimizer (reported)
};

// Triangles: three indices each, counter-clockwise seen from the outside; normals per vertex or
// none.
struct InputMesh {
    std::vector<Core::Vec3> positions;
    std::vector<Core::Vec3> normals;
    std::vector<std::uint32_t> indices;
};

// An asset as Godot imported it.
struct InputAsset {
    Core::AssetId id;                // from Godot's stable UID
    Content::ContentId name;         // game:..., from its path in the project
    std::string source;              // the imported file: res://Content/Rocks/test_rock.glb
    std::vector<InputMesh> surfaces; // the visual mesh, in the asset's own space
    CollisionSource collision = CollisionSource::VisualMesh;
    std::vector<InputMesh> collisionMeshes; // CollisionMeshes: in the asset's own space
};

// An asset placed in the scene: the node that places it, and its pose relative to the scene's
// origin.
struct InputPlacement {
    std::string node; // the node's path in the scene: Rocks/TestRock2
    Core::AssetId asset;
    Core::Vec3 position{0.0f};
    Core::Quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    Core::Vec3 scale{1.0f};
};

// A RunicCollider3D: a box, sphere or capsule placed on its own, relative to the scene's origin.
struct InputCollider {
    std::string node;
    Assets::CollisionShape shape; // its pose relative to the scene's origin; never a mesh
};

// One scene: in slice 9 it cooks into one cell around its origin (64 m cells are COOK-3's).
struct CookScene {
    Content::ContentId name; // game:test_region
    std::string source;      // res://Content/TestRegion/test_region.tscn
    Core::WorldPosition origin;
    std::vector<InputAsset> assets;
    std::vector<InputPlacement> placements;
    std::vector<InputCollider> colliders;
};

} // namespace Runic::Cooker
