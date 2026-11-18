// GDT-4 with Godot: the Cooker integration in the editor, headless, in this process. Godot imports
// the test region's glTF (a mesh "made in Blender": tests/godot/fixtures/test_region) through the
// import plugin; the reader turns the scene into the Cooker's input (the naming convention, the
// collider marker, Godot's triangles turned counter-clockwise); the cook's collision loads into the
// local world, where a character walks up the ramp onto the platform and stops at the collider,
// and the "see what the server sees" layer draws it where Godot draws the visual.

#include "EditorCook.hpp"
#include "EditorHarness.hpp"
#include "PhysicsBridge.hpp"
#include "WorldComponents.hpp"

#include <godot_cpp/classes/editor_file_system.hpp>
#include <godot_cpp/classes/editor_interface.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/mesh.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/packed_scene.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/window.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/variant/packed_vector3_array.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <limits>
#include <sstream>
#include <string>

using namespace Runic;
using Core::Vec3;
using Core::WorldPosition;
using GodotTools::Testing::EditorHarness;
using GodotTools::Testing::ScratchProject;
namespace fs = std::filesystem;

namespace {

const WorldPosition kSceneOrigin{5'000'000.0, 120.0, -7'000'000.0};
constexpr const char *kGlb = "res://Content/TestRegion/test_region.glb";
constexpr const char *kScene = "res://Content/TestRegion/test_region.tscn";
constexpr const char *kRegionInProject = "Content/TestRegion";
constexpr float kToleranceM = 1e-3f;

// A scratch project with the test region in Content/TestRegion.
class RegionProject : public ScratchProject {
  public:
    explicit RegionProject(std::string_view name) : ScratchProject(name) {
        if (!Made()) {
            return;
        }
        fs::create_directories(Path() / kRegionInProject);
        for (const char *file : {"test_region.glb", "test_region.tscn"}) {
            fs::copy_file(fs::path(RUNIC_TEST_REGION_DIR) / file, Path() / kRegionInProject / file);
        }
    }
    fs::path Cooked() const { return Path() / "Build/Cooked"; }
};

bool Imported() {
    godot::EditorInterface *editor = godot::EditorInterface::get_singleton();
    if (editor == nullptr) {
        return false;
    }
    godot::EditorFileSystem *files = editor->get_resource_filesystem();
    return !files->is_scanning() && !files->is_importing() &&
           godot::ResourceLoader::get_singleton()->exists(kGlb);
}

void RunRealtimeTick(Simulation::SimulationRuntime &runtime) {
    const std::uint64_t before = runtime.TicksRun(Simulation::TickDomain::Hz30);
    while (runtime.TicksRun(Simulation::TickDomain::Hz30) == before) {
        runtime.RunMasterTick();
    }
}

const Cooker::InputAsset *AssetNamed(const Cooker::CookScene &scene, std::string_view name) {
    for (const Cooker::InputAsset &asset : scene.assets) {
        if (asset.name.Text() == name) {
            return &asset;
        }
    }
    return nullptr;
}

const Cooker::InputPlacement *PlacementOf(const Cooker::CookScene &scene, std::string_view node) {
    for (const Cooker::InputPlacement &placement : scene.placements) {
        if (placement.node == node) {
            return &placement;
        }
    }
    return nullptr;
}

std::size_t Triangles(const std::vector<Cooker::InputMesh> &meshes) {
    std::size_t triangles = 0;
    for (const Cooker::InputMesh &mesh : meshes) {
        triangles += mesh.indices.size() / 3;
    }
    return triangles;
}

godot::Node *PhysicsDock() {
    auto *tree =
        godot::Object::cast_to<godot::SceneTree>(godot::Engine::get_singleton()->get_main_loop());
    return tree != nullptr ? tree->get_root()->find_child("Runic Physics", true, false) : nullptr;
}

struct Bounds {
    Vec3 min{std::numeric_limits<float>::max()};
    Vec3 max{std::numeric_limits<float>::lowest()};
};

} // namespace

TEST(EditorCook, ReadsWhatGodotImportedWithTheConventionAndTheMarker) {
    RUNIC_REQUIRE_LIBGODOT();
    RegionProject project("cook-read");
    ASSERT_TRUE(project.Made());
    Simulation::SimulationRuntime world;
    EditorHarness editor(project.Path(), world, kSceneOrigin);
    ASSERT_TRUE(editor.Start());
    ASSERT_TRUE(editor.RunUntil(Imported));

    const auto read = GodotTools::ReadImportedScene(kScene, kSceneOrigin);
    ASSERT_TRUE(read) << read.GetError().message;
    const Cooker::CookScene &scene = read.Value();
    EXPECT_EQ(scene.name.Text(), "game:test_region/test_region");
    EXPECT_EQ(scene.origin, kSceneOrigin);
    ASSERT_EQ(scene.assets.size(), 4u); // Rock-rcol is not an asset: it is Rock's collision
    const Cooker::InputAsset *platform = AssetNamed(scene, "game:test_region/test_region/platform");
    const Cooker::InputAsset *rock = AssetNamed(scene, "game:test_region/test_region/rock");
    ASSERT_NE(platform, nullptr);
    ASSERT_NE(rock, nullptr);
    EXPECT_EQ(platform->source, kGlb);
    EXPECT_EQ(platform->collision, Cooker::CollisionSource::VisualMesh);
    EXPECT_EQ(Triangles(platform->surfaces), 12u);
    // The naming convention: the rock's collision is its 1.4 m box, in the rock's own space.
    EXPECT_EQ(rock->collision, Cooker::CollisionSource::CollisionMeshes);
    ASSERT_EQ(rock->collisionMeshes.size(), 1u);
    EXPECT_EQ(Triangles(rock->collisionMeshes), 12u);
    float top = 0.0f;
    for (const Vec3 &p : rock->collisionMeshes[0].positions) {
        top = std::max(top, p.y);
    }
    EXPECT_NEAR(top, 1.1f, kToleranceM);

    // Godot's clockwise triangles come back counter-clockwise: each one faces its normals.
    const Cooker::InputMesh &surface = platform->surfaces[0];
    for (std::size_t t = 0; t < surface.indices.size(); t += 3) {
        const Vec3 a = surface.positions[surface.indices[t]];
        const Vec3 b = surface.positions[surface.indices[t + 1]];
        const Vec3 c = surface.positions[surface.indices[t + 2]];
        const Vec3 facing = glm::normalize(glm::cross(b - a, c - a));
        EXPECT_GT(glm::dot(facing, surface.normals[surface.indices[t]]), 0.99f)
            << "triangle " << t / 3;
    }

    ASSERT_EQ(scene.placements.size(), 4u);
    ASSERT_NE(PlacementOf(scene, "Geometry/Rock"), nullptr);
    EXPECT_EQ(PlacementOf(scene, "Geometry/Rock")->position, Vec3(2, 1, 3));
    EXPECT_EQ(PlacementOf(scene, "Geometry/Rock")->asset, rock->id);
    ASSERT_EQ(scene.colliders.size(), 1u);
    EXPECT_EQ(scene.colliders[0].node, "Wall");
    EXPECT_EQ(scene.colliders[0].shape.kind, Assets::CollisionShapeKind::Box);
    EXPECT_NEAR(scene.colliders[0].shape.halfExtents.x, 0.1f, kToleranceM);
    EXPECT_NEAR(scene.colliders[0].shape.halfExtents.z, 5.0f, kToleranceM);
    EXPECT_EQ(scene.colliders[0].shape.position, Vec3(4, 3, 0));

    // The same file, the same IDs: they come from Godot's UID for it.
    const auto again = GodotTools::ReadImportedScene(kScene, kSceneOrigin);
    ASSERT_TRUE(again);
    EXPECT_EQ(AssetNamed(again.Value(), "game:test_region/test_region/rock")->id, rock->id);
    EXPECT_NE(platform->id, rock->id);

    // What Godot draws: the platform's mesh, and the collision-only mesh hidden by the import
    // plugin.
    const godot::Ref<godot::PackedScene> glb = godot::ResourceLoader::get_singleton()->load(kGlb);
    ASSERT_TRUE(glb.is_valid());
    godot::Node *instance = glb->instantiate();
    ASSERT_NE(instance, nullptr);
    auto *drawn =
        godot::Object::cast_to<godot::MeshInstance3D>(instance->get_node_or_null("Platform"));
    auto *hidden =
        godot::Object::cast_to<godot::MeshInstance3D>(instance->get_node_or_null("Rock/Rock-rcol"));
    ASSERT_NE(drawn, nullptr);
    ASSERT_NE(hidden, nullptr);
    EXPECT_TRUE(drawn->is_visible());
    EXPECT_NEAR(drawn->get_mesh()->get_aabb().size.x, 10.0f, kToleranceM);
    EXPECT_FALSE(hidden->is_visible());
    EXPECT_TRUE(hidden->has_meta(godot::String(GodotTools::kCollisionOnlyMeta.data())));
    memdelete(instance);
}

TEST(EditorCook, ACookedRegionIsWalkableAndItsCollisionDrawsWhereGodotDrawsTheVisual) {
    RUNIC_REQUIRE_LIBGODOT();
    RegionProject project("cook-walk");
    ASSERT_TRUE(project.Made());
    Simulation::SimulationRuntime world;
    Simulation::PhysicsBridge &bridge = world.EnablePhysics();
    EditorHarness editor(project.Path(), world, kSceneOrigin);
    ASSERT_TRUE(editor.Start());
    ASSERT_TRUE(editor.RunUntil(Imported));

    GodotTools::EditorCookSpec spec;
    spec.scenes = {kScene};
    spec.output = project.Cooked();
    spec.origin = kSceneOrigin;
    const auto report = GodotTools::CookEditorScenes(spec);
    ASSERT_TRUE(report) << report.GetError().message;
    ASSERT_TRUE(report.Value().succeeded)
        << (report.Value().problems.empty() ? "" : report.Value().problems[0]);
    EXPECT_EQ(report.Value().meshes, 4u);
    EXPECT_EQ(report.Value().collisions, 4u);
    EXPECT_EQ(report.Value().placements, 4u);
    EXPECT_EQ(report.Value().colliders, 1u);
    EXPECT_EQ(report.Value().collisionTrianglesOut,
              44u); // ground 12, ramp 8, platform 12, rock's box 12

    const auto loaded = GodotTools::LoadCookedCells(world, project.Cooked());
    ASSERT_TRUE(loaded) << loaded.GetError().message;
    EXPECT_EQ(loaded.Value().cells, 1u);
    EXPECT_EQ(loaded.Value().bodies, 5u);
    // A second load replaces the first.
    const auto reloaded = GodotTools::LoadCookedCells(world, project.Cooked());
    ASSERT_TRUE(reloaded);
    EXPECT_EQ(reloaded.Value().removed, 5u);

    // Walkable: a character from the ground up the ramp onto the platform, to the wall.
    Physics::CharacterDesc walkerDesc;
    walkerDesc.layer = bridge.World().Layers().Find(Physics::kCharacterLayerName);
    const ECS::Entity walker = world.Entities().Create();
    world.Entities().Set(walker, World::WorldTransform{kSceneOrigin + Vec3(-12.0f, 0.0f, 0.0f)});
    bridge.AddCharacter(walker, walkerDesc);
    constexpr int kEightSeconds = 240;
    for (int tick = 0; tick < kEightSeconds; ++tick) {
        if (auto *character = world.Entities().TryGet<Simulation::PhysicsCharacter>(walker)) {
            character->move = {Vec3(3.0f, 0.0f, 0.0f)};
        }
        RunRealtimeTick(world);
    }
    const auto *character = world.Entities().TryGet<Simulation::PhysicsCharacter>(walker);
    ASSERT_NE(character, nullptr);
    const Physics::CharacterState state = bridge.World().Character(character->handle);
    EXPECT_EQ(state.ground, Physics::GroundState::OnGround);
    EXPECT_NEAR(state.pose.position.y - kSceneOrigin.y, 1.0, 0.02); // on the platform
    EXPECT_NEAR(state.pose.position.x - kSceneOrigin.x, 3.6, 0.05); // stopped by the wall
    EXPECT_EQ(bridge.World().Stats().bodies, 5u);                   // the first load's bodies went

    // The server view draws the cooked collision where Godot draws the visual: the ground's 30 m,
    // read back from Godot, in the edited scene's space.
    godot::Node *dock = nullptr;
    Bounds drawn;
    ASSERT_TRUE(editor.RunUntil([&] {
        dock = PhysicsDock();
        if (dock == nullptr) {
            return false;
        }
        const godot::PackedVector3Array lines = dock->call("server_view_surface", 1, 0);
        drawn = Bounds{};
        for (int64_t i = 0; i < lines.size(); ++i) {
            drawn.min = glm::min(drawn.min, Vec3(lines[i].x, lines[i].y, lines[i].z));
            drawn.max = glm::max(drawn.max, Vec3(lines[i].x, lines[i].y, lines[i].z));
        }
        return drawn.max.x > 14.0f;
    }));
    const godot::Ref<godot::PackedScene> region =
        godot::ResourceLoader::get_singleton()->load(kGlb);
    godot::Node *instance = region->instantiate();
    auto *ground =
        godot::Object::cast_to<godot::MeshInstance3D>(instance->get_node_or_null("Ground"));
    ASSERT_NE(ground, nullptr);
    const godot::AABB visual = ground->get_mesh()->get_aabb();
    EXPECT_NEAR(drawn.min.x, visual.position.x, kToleranceM);
    EXPECT_NEAR(drawn.max.x, visual.position.x + visual.size.x, kToleranceM);
    EXPECT_NEAR(drawn.min.z, visual.position.z, kToleranceM);
    EXPECT_NEAR(drawn.min.y, visual.position.y, kToleranceM);
    EXPECT_NEAR(drawn.max.y, 5.0f, kToleranceM); // the wall collider's top
    EXPECT_TRUE(static_cast<bool>(dock->call("server_view_drawn_over_visuals")));
    memdelete(instance);
}

TEST(EditorCook, TheImportPresetDecidesTheCollisionOfMeshesWithoutCollisionMeshes) {
    RUNIC_REQUIRE_LIBGODOT();
    RegionProject project("cook-preset");
    ASSERT_TRUE(project.Made());
    Simulation::SimulationRuntime world;
    EditorHarness editor(project.Path(), world, kSceneOrigin);
    ASSERT_TRUE(editor.Start());
    ASSERT_TRUE(editor.RunUntil(Imported));

    // The Import dock's "Runic › Collision: None" for the file, as Godot saves it, then a reimport.
    const fs::path importFile = project.Path() / kRegionInProject / "test_region.glb.import";
    std::stringstream text;
    text << std::ifstream(importFile).rdbuf();
    std::string settings = text.str();
    // Godot wrote the option's default (0) when it first imported the file: it becomes 1.
    const std::string option = std::string(GodotTools::kCollisionPresetOption) + "=";
    const std::size_t at = settings.find(option);
    ASSERT_NE(at, std::string::npos) << settings;
    settings.replace(at, settings.find('\n', at) - at, option + "1");
    std::ofstream(importFile) << settings;
    godot::PackedStringArray files;
    files.push_back(kGlb);
    godot::EditorInterface::get_singleton()->get_resource_filesystem()->reimport_files(files);
    ASSERT_TRUE(editor.RunUntil(Imported));

    const auto read = GodotTools::ReadImportedScene(kScene, kSceneOrigin);
    ASSERT_TRUE(read) << read.GetError().message;
    EXPECT_EQ(AssetNamed(read.Value(), "game:test_region/test_region/platform")->collision,
              Cooker::CollisionSource::None);
    EXPECT_EQ(AssetNamed(read.Value(), "game:test_region/test_region/ground")->collision,
              Cooker::CollisionSource::None);
    // Collision meshes the artist made win over the file's preset.
    EXPECT_EQ(AssetNamed(read.Value(), "game:test_region/test_region/rock")->collision,
              Cooker::CollisionSource::CollisionMeshes);

    GodotTools::EditorCookSpec spec;
    spec.scenes = {kScene};
    spec.output = project.Cooked();
    const auto report = GodotTools::CookEditorScenes(spec);
    ASSERT_TRUE(report);
    EXPECT_TRUE(report.Value().succeeded);
    EXPECT_EQ(report.Value().collisions, 1u);
    EXPECT_EQ(report.Value().placements, 4u);
}

TEST(EditorCook, ContentNamesComeFromTheProjectPathInSnakeCase) {
    EXPECT_EQ(GodotTools::ContentNameOf("res://Content/TestRegion/test_region.glb", "Platform",
                                        "Content"),
              "game:test_region/test_region/platform");
    EXPECT_EQ(
        GodotTools::ContentNameOf("res://Content/Rocks/BigRock 02.glb", "Rocks/RockA-2", "Content"),
        "game:rocks/big_rock_02/rocks/rock_a_2");
    EXPECT_EQ(GodotTools::ContentNameOf("res://Other/thing.tscn", "", "Content"),
              "game:other/thing");
    EXPECT_EQ(GodotTools::ContentNameOf("res://Content/HTTPServer.glb", "", "Content"),
              "game:httpserver");
}
