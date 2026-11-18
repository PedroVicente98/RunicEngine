// COOK-1: the first cooker proof without Godot. A scene as GodotTools hands it over (an imported
// mesh, its placement 8,600 km out, a collider) cooks into a mesh, a collision and a cell that
// Runic::Assets reads back; a character walks on the cooked collision and stops at the collider.
// The collision naming convention, welding, scaled placements, stable IDs, and a wrong scene that
// cooks nothing.

#include "Cooker.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <filesystem>
#include <format>
#include <string>
#include <vector>

#include "AssetCache.hpp"
#include "PhysicsWorld.hpp"

using namespace Runic;
using namespace Runic::Cooker;
using Core::AssetId;
using Core::Vec3;
using Core::WorldPosition;
namespace fs = std::filesystem;

namespace {

const WorldPosition kFarAway{5'000'000.0, 120.0, -7'000'000.0};
constexpr float kStepSeconds = 1.0f / 30.0f;
constexpr int kFourSeconds = 120;

// A box from min to max as 12 outward triangles, with flat normals per corner left out.
InputMesh Box(Vec3 min, Vec3 max) {
    InputMesh mesh;
    mesh.positions = {{min.x, min.y, min.z}, {max.x, min.y, min.z}, {max.x, max.y, min.z},
                      {min.x, max.y, min.z}, {min.x, min.y, max.z}, {max.x, min.y, max.z},
                      {max.x, max.y, max.z}, {min.x, max.y, max.z}};
    mesh.indices = {0, 2, 1, 0, 3, 2, 4, 5, 6, 4, 6, 7, 0, 1, 5, 0, 5, 4,
                    3, 7, 6, 3, 6, 2, 0, 4, 7, 0, 7, 3, 1, 2, 6, 1, 6, 5};
    return mesh;
}

// The test region: a 10 m square platform 1 m high (the "Blender mesh"), placed at the scene's
// origin 8,600 km out, and a wall collider standing on it at x = 4.
CookScene TestRegion() {
    InputAsset platform;
    platform.id = AssetId(0x5eed'0001);
    platform.name = Content::ContentId::Of("game:test_region/platform");
    platform.source = "res://Content/TestRegion/test_region.glb";
    platform.surfaces = {Box({-5, 0, -5}, {5, 1, 5})};
    platform.collision = CollisionSource::VisualMesh;

    InputPlacement placed;
    placed.node = "Platform";
    placed.asset = platform.id;

    InputCollider wall;
    wall.node = "Wall";
    wall.shape.kind = Assets::CollisionShapeKind::Box;
    wall.shape.halfExtents = {0.1f, 2.0f, 5.0f};
    wall.shape.position = {4.0f, 3.0f, 0.0f};

    CookScene scene;
    scene.name = Content::ContentId::Of("game:test_region");
    scene.source = "res://Content/TestRegion/test_region.tscn";
    scene.origin = kFarAway;
    scene.assets = {platform};
    scene.placements = {placed};
    scene.colliders = {wall};
    return scene;
}

class CookedFolder {
  public:
    explicit CookedFolder(std::string_view name)
        : m_Root(fs::temp_directory_path() / ("runic-cooker-test-" + std::string(name))) {
        fs::remove_all(m_Root);
    }
    ~CookedFolder() {
        std::error_code ignored;
        fs::remove_all(m_Root, ignored);
    }
    CookSettings Settings() const {
        CookSettings settings;
        settings.output = m_Root;
        return settings;
    }
    const fs::path &Root() const { return m_Root; }

  private:
    fs::path m_Root;
};

std::string Problems(const CookReport &report) {
    std::string all;
    for (const std::string &problem : report.problems) {
        all += problem + "\n";
    }
    return all;
}

// What a runtime does with a cooked cell, in a test's own words: static bodies for its placements'
// collisions and its colliders, at the cell's origin plus their offsets.
std::uint32_t LoadCell(Assets::AssetCache &cache, Core::AssetId cellId,
                       Physics::PhysicsWorld &world) {
    const auto cell = cache.LoadCell(cellId);
    EXPECT_TRUE(cell) << cell.GetError().message;
    if (!cell) {
        return 0;
    }
    const Physics::CollisionLayer statics = world.Layers().Find(Physics::kStaticLayerName);
    std::uint32_t bodies = 0;
    const auto place = [&](const Assets::CollisionShape &shape, const Assets::CookedCollision *of,
                           Vec3 at, Core::Quat rotation, std::string name) {
        Physics::ShapeDesc desc;
        switch (shape.kind) {
        case Assets::CollisionShapeKind::Box:
            desc = Physics::BoxShape(shape.halfExtents);
            break;
        case Assets::CollisionShapeKind::Sphere:
            desc = Physics::SphereShape(shape.radius);
            break;
        case Assets::CollisionShapeKind::Capsule:
            desc = Physics::CapsuleShape(shape.halfHeight, shape.radius);
            break;
        case Assets::CollisionShapeKind::Mesh:
            desc = Physics::MeshShape(of->meshes[shape.mesh].positions,
                                      of->meshes[shape.mesh].indices);
            break;
        }
        Physics::BodyDesc body;
        body.shape = world.AddShape(name, desc).Value();
        body.layer = statics;
        body.pose = {cell.Value()->origin + (at + rotation * shape.position),
                     rotation * shape.rotation};
        world.CreateBody(body);
        ++bodies;
    };
    for (const Assets::CellPlacement &placement : cell.Value()->placements) {
        if (!placement.collision.IsValid()) {
            continue;
        }
        const auto collision = cache.LoadCollision(placement.collision);
        EXPECT_TRUE(collision) << collision.GetError().message;
        for (std::size_t i = 0; i < collision.Value()->shapes.size(); ++i) {
            place(collision.Value()->shapes[i], &collision.Value().Get(), placement.position,
                  placement.rotation, std::format("engine:test_placement_{}_{}", placement.id, i));
        }
    }
    for (const Assets::CellCollider &collider : cell.Value()->colliders) {
        place(collider.shape, nullptr, Vec3(0.0f), Core::Quat(1, 0, 0, 0),
              std::format("engine:test_collider_{}", collider.id));
    }
    return bodies;
}

} // namespace

TEST(Cooker, CooksAMeshItsCollisionAndACellThatAssetsReads) {
    CookedFolder folder("region");
    const CookScene scene = TestRegion();
    const CookReport report = Cook({&scene, 1}, folder.Settings());
    ASSERT_TRUE(report.succeeded) << Problems(report);
    EXPECT_EQ(report.meshes, 1u);
    EXPECT_EQ(report.collisions, 1u);
    EXPECT_EQ(report.cells, 1u);
    EXPECT_EQ(report.placements, 1u);
    EXPECT_EQ(report.colliders, 1u);
    EXPECT_EQ(report.collisionTrianglesOut, 12u);
    EXPECT_EQ(report.written.size(), 4u); // three files and the catalog

    Assets::AssetCache cache(folder.Root());
    ASSERT_TRUE(cache.Open());
    EXPECT_EQ(cache.Catalog().Count(), 3u);
    const auto mesh = cache.LoadMesh("game:test_region/platform");
    ASSERT_TRUE(mesh) << mesh.GetError().message;
    EXPECT_EQ(mesh.Value()->indices.size(), 36u);
    EXPECT_EQ(mesh.Value()->normals.size(), mesh.Value()->positions.size()); // computed
    EXPECT_EQ(mesh.Value()->boundsMax, Vec3(5, 1, 5));
    const auto cell = cache.LoadCell(CellIdOf(scene.name));
    ASSERT_TRUE(cell) << cell.GetError().message;
    EXPECT_EQ(cell.Value()->origin, kFarAway);
    ASSERT_EQ(cell.Value()->placements.size(), 1u);
    EXPECT_EQ(cell.Value()->placements[0].mesh, scene.assets[0].id);
    EXPECT_EQ(cell.Value()->placements[0].collision, CollisionIdOf(scene.assets[0].id, Vec3(1)));
    EXPECT_EQ(cell.Value()->placements[0].id, PlacementIdOf(scene.name, "Platform"));
    ASSERT_EQ(cell.Value()->colliders.size(), 1u);
    EXPECT_EQ(cell.Value()->colliders[0].shape.position, Vec3(4, 3, 0));
    const Assets::AssetEntry *collision =
        cache.Catalog().Find(CollisionIdOf(scene.assets[0].id, Vec3(1)));
    ASSERT_NE(collision, nullptr);
    EXPECT_EQ(collision->name.Text(), "game:test_region/platform/collision");
}

TEST(Cooker, ACharacterWalksOnTheCookedCollisionAndStopsAtTheCollider) {
    CookedFolder folder("walk");
    const CookScene scene = TestRegion();
    ASSERT_TRUE(Cook({&scene, 1}, folder.Settings()).succeeded);
    Assets::AssetCache cache(folder.Root());
    ASSERT_TRUE(cache.Open());
    Physics::PhysicsWorld world;
    EXPECT_EQ(LoadCell(cache, CellIdOf(scene.name), world), 2u);

    Physics::CharacterDesc walker;
    walker.layer = world.Layers().Find(Physics::kCharacterLayerName);
    walker.pose.position = kFarAway + Vec3(-3.0f, 1.0f, 0.0f); // on the platform
    const auto character = world.CreateCharacter(walker);
    ASSERT_TRUE(character);
    for (int step = 0; step < kFourSeconds; ++step) {
        world.MoveCharacter(character.Value(), {Vec3(2.0f, 0.0f, 0.0f)});
        world.Step(kStepSeconds);
    }
    const Physics::CharacterState state = world.Character(character.Value());
    EXPECT_EQ(state.ground, Physics::GroundState::OnGround);
    // On the cooked platform's top, 1 m up, and stopped by the wall at x = 4 (its face at 3.9,
    // the capsule's radius 0.3 away).
    EXPECT_NEAR(state.pose.position.y - kFarAway.y, 1.0, 0.01);
    EXPECT_NEAR(state.pose.position.x - kFarAway.x, 3.6, 0.05);
}

TEST(Cooker, CollisionMeshesUnderTheConventionReplaceTheVisualMesh) {
    CookedFolder folder("collision-meshes");
    CookScene scene = TestRegion();
    // A detailed visual (a box split into many small triangles stands in for it here: the same box
    // twice) and a one-box collision mesh.
    scene.assets[0].surfaces.push_back(Box({-5, 1, -5}, {-4, 2, -4}));
    scene.assets[0].collision = CollisionSource::CollisionMeshes;
    scene.assets[0].collisionMeshes = {Box({-5, 0, -5}, {5, 1.2f, 5})};
    const CookReport report = Cook({&scene, 1}, folder.Settings());
    ASSERT_TRUE(report.succeeded) << Problems(report);
    EXPECT_EQ(report.collisionTrianglesOut, 12u);
    Assets::AssetCache cache(folder.Root());
    ASSERT_TRUE(cache.Open());
    const auto collision = cache.LoadCollision(CollisionIdOf(scene.assets[0].id, Vec3(1)));
    ASSERT_TRUE(collision);
    ASSERT_EQ(collision.Value()->meshes.size(), 1u);
    float top = 0.0f;
    for (const Vec3 &p : collision.Value()->meshes[0].positions) {
        top = std::max(top, p.y);
    }
    EXPECT_FLOAT_EQ(top, 1.2f);
    EXPECT_EQ(cache.LoadMesh(scene.assets[0].id).Value()->indices.size(), 72u); // both surfaces
}

TEST(Cooker, WeldsVerticesAndDropsDegenerateTriangles) {
    CookedFolder folder("weld");
    CookScene scene = TestRegion();
    // Every triangle with its own three corners, as an exporter may write them, a twin vertex 0.02
    // mm off, and two triangles of no area.
    InputMesh loose;
    const InputMesh box = Box({-5, 0, -5}, {5, 1, 5});
    for (const std::uint32_t index : box.indices) {
        loose.indices.push_back(static_cast<std::uint32_t>(loose.positions.size()));
        loose.positions.push_back(box.positions[index] +
                                  Vec3(0.00002f * static_cast<float>(index % 2), 0, 0));
    }
    const auto at = static_cast<std::uint32_t>(loose.positions.size());
    loose.positions.insert(loose.positions.end(), {{0, 1, 0}, {1, 1, 0}, {2, 1, 0}});
    loose.indices.insert(loose.indices.end(),
                         {at, at + 1, at + 2, at, at, at + 1}); // a line, a point
    scene.assets[0].surfaces = {loose};
    const CookReport report = Cook({&scene, 1}, folder.Settings());
    ASSERT_TRUE(report.succeeded) << Problems(report);
    EXPECT_EQ(report.collisionTrianglesIn, 14u);
    EXPECT_EQ(report.collisionTrianglesOut, 12u);
    EXPECT_EQ(report.droppedTriangles, 2u);
    Assets::AssetCache cache(folder.Root());
    ASSERT_TRUE(cache.Open());
    const auto collision = cache.LoadCollision(CollisionIdOf(scene.assets[0].id, Vec3(1)));
    ASSERT_TRUE(collision);
    EXPECT_EQ(collision.Value()->meshes[0].positions.size(), 8u); // the box's corners, welded
    EXPECT_EQ(report.weldedVertices, 36u + 3u - 8u - 3u);         // what merged into another
}

TEST(Cooker, AScaledPlacementGetsItsOwnCollisionAndEqualScalesShareOne) {
    CookedFolder folder("scale");
    CookScene scene = TestRegion();
    InputPlacement twice = scene.placements[0];
    twice.node = "PlatformTwiceAsBig";
    twice.position = {30, 0, 0};
    twice.scale = {2, 2, 2};
    InputPlacement againTwice = twice;
    againTwice.node = "AnotherTwiceAsBig";
    againTwice.position = {60, 0, 0};
    scene.placements.push_back(twice);
    scene.placements.push_back(againTwice);
    const CookReport report = Cook({&scene, 1}, folder.Settings());
    ASSERT_TRUE(report.succeeded) << Problems(report);
    EXPECT_EQ(report.collisions, 2u); // scale 1 and scale 2
    Assets::AssetCache cache(folder.Root());
    ASSERT_TRUE(cache.Open());
    const AssetId scaled = CollisionIdOf(scene.assets[0].id, Vec3(2));
    EXPECT_NE(scaled, CollisionIdOf(scene.assets[0].id, Vec3(1)));
    const auto cell = cache.LoadCell(CellIdOf(scene.name));
    ASSERT_TRUE(cell);
    EXPECT_EQ(cell.Value()->placements[1].collision, scaled);
    EXPECT_EQ(cell.Value()->placements[2].collision, scaled);
    const auto collision = cache.LoadCollision(scaled);
    ASSERT_TRUE(collision);
    float top = 0.0f;
    for (const Vec3 &p : collision.Value()->meshes[0].positions) {
        top = std::max(top, p.y);
    }
    EXPECT_FLOAT_EQ(top, 2.0f); // baked in: the runtime never scales
    EXPECT_EQ(cache.Catalog().Find(scaled)->name.Text(),
              "game:test_region/platform/collision_2000_2000_2000");
}

TEST(Cooker, IdsAreStableFromCookToCook) {
    const Content::ContentId region = Content::ContentId::Of("game:test_region");
    EXPECT_EQ(PlacementIdOf(region, "Rocks/Rock2"), PlacementIdOf(region, "Rocks/Rock2"));
    EXPECT_NE(PlacementIdOf(region, "Rocks/Rock2"), PlacementIdOf(region, "Rocks/Rock3"));
    EXPECT_NE(PlacementIdOf(region, "Rocks/Rock2"),
              PlacementIdOf(Content::ContentId::Of("game:test_other_region"), "Rocks/Rock2"));
    EXPECT_NE(PlacementIdOf(region, "Rocks/Rock2"), 0u);
    EXPECT_EQ(CellIdOf(region), CellIdOf(Content::ContentId::Of("game:test_region")));
    EXPECT_EQ(CollisionIdOf(AssetId(7), Vec3(1)), CollisionIdOf(AssetId(7), Vec3(1)));

    // Two cooks of the same scene write the same bytes.
    CookedFolder first("stable-1");
    CookedFolder second("stable-2");
    const CookScene scene = TestRegion();
    ASSERT_TRUE(Cook({&scene, 1}, first.Settings()).succeeded);
    ASSERT_TRUE(Cook({&scene, 1}, second.Settings()).succeeded);
    for (const fs::directory_entry &file : fs::recursive_directory_iterator(first.Root())) {
        if (file.is_regular_file()) {
            const fs::path relative = fs::relative(file.path(), first.Root());
            EXPECT_EQ(Assets::ReadBytesFromFile(file.path()).Value(),
                      Assets::ReadBytesFromFile(second.Root() / relative).Value())
                << relative;
        }
    }
}

TEST(Cooker, AWrongSceneCooksNothingAndSaysWhatAndWhere) {
    CookedFolder folder("wrong");
    // A good cook first: a failed one must leave it as it was.
    const CookScene good = TestRegion();
    ASSERT_TRUE(Cook({&good, 1}, folder.Settings()).succeeded);
    const auto before = Assets::ReadBytesFromFile(folder.Root() / "catalog.txt").Value();

    CookScene scene = TestRegion();
    InputPlacement stray = scene.placements[0];
    stray.node = "Stray";
    stray.asset = AssetId(0xdead);
    InputPlacement flat = scene.placements[0];
    flat.node = "Flat";
    flat.scale = {1, 0, 1};
    InputPlacement lost = scene.placements[0];
    lost.node = "Lost";
    lost.position.y = std::nanf("");
    scene.placements.insert(scene.placements.end(), {stray, flat, lost});
    scene.colliders[0].shape.halfExtents.y = 0.0f;
    InputAsset hull = scene.assets[0];
    hull.id = AssetId(0x5eed'0002);
    hull.name = Content::ContentId::Of("game:test_region/hull");
    hull.collision = CollisionSource::ConvexHull;
    InputAsset twin = scene.assets[0];
    twin.name = Content::ContentId::Of("game:test_region/twin");
    InputAsset empty = scene.assets[0];
    empty.id = AssetId(0x5eed'0003);
    empty.name = Content::ContentId::Of("game:test_region/empty");
    empty.surfaces = {};
    scene.assets.insert(scene.assets.end(), {hull, twin, empty});

    const CookReport report = Cook({&scene, 1}, folder.Settings());
    EXPECT_FALSE(report.succeeded);
    const std::string all = Problems(report);
    const auto says = [&](const std::string &part) { return all.find(part) != std::string::npos; };
    EXPECT_TRUE(says("node Stray")) << all;
    EXPECT_TRUE(says("000000000000dead")) << all;
    EXPECT_TRUE(says("node Flat")) << all;
    EXPECT_TRUE(says("node Lost")) << all;
    EXPECT_TRUE(says("node Wall")) << all;
    EXPECT_TRUE(says("convex")) << all;
    EXPECT_TRUE(says("game:test_region/twin")) << all; // the same ID as the platform
    EXPECT_TRUE(says("game:test_region/empty")) << all;
    EXPECT_TRUE(says("res://Content/TestRegion/test_region.tscn")) << all;
    EXPECT_EQ(report.problems.size(), 7u) << all;
    EXPECT_TRUE(report.written.empty());
    EXPECT_EQ(Assets::ReadBytesFromFile(folder.Root() / "catalog.txt").Value(), before);
}
