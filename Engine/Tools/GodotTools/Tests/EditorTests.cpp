// GDT-2 with Godot: the RunicEditor's tools in the editor library, headless, in this process. The
// editor opens a project made from the template, loads its editor library, and reloads it when a
// build replaces the file.

#include "EditorHarness.hpp"

#include <godot_cpp/classes/class_db_singleton.hpp>

#include <gtest/gtest.h>

#include <filesystem>

using namespace Runic;
using GodotTools::Testing::EditorHarness;
using GodotTools::Testing::ScratchProject;
namespace fs = std::filesystem;

namespace {

constexpr const char *kFixtureClass = "RunicFixtureNode";
constexpr const char *kBuildIdMethod = "build_id";
constexpr const char *kLibraryInProject = "Build/bin/libgame_editor.so";

// The build of the fixture library Godot has loaded: 0 when none is.
int64_t LoadedFixtureBuild() {
    godot::ClassDBSingleton *classes = godot::ClassDBSingleton::get_singleton();
    if (classes == nullptr || !classes->class_exists(kFixtureClass)) {
        return 0;
    }
    const godot::Variant made = classes->instantiate(kFixtureClass);
    godot::Object *node = made;
    if (node == nullptr) {
        return 0;
    }
    const int64_t build = node->call(kBuildIdMethod);
    node->call("free");
    return build;
}

// Replaces a file the way a build does: a new file, renamed over the old one.
void Replace(const fs::path &target, const fs::path &with) {
    const fs::path staging = target.string() + ".new";
    fs::copy_file(with, staging, fs::copy_options::overwrite_existing);
    fs::rename(staging, target);
}

} // namespace

TEST(GameEditorLibrary, TheEditorLoadsTheProjectsEditorLibrary) {
    RUNIC_REQUIRE_LIBGODOT();
    ScratchProject project("load");
    ASSERT_TRUE(project.Made());
    fs::create_directories(project.Path() / "Build/bin");
    fs::copy_file(RUNIC_FIXTURE_BUILD_1, project.Path() / kLibraryInProject);
    Simulation::SimulationRuntime world;
    EditorHarness editor(project.Path(), world);
    ASSERT_TRUE(editor.Start());
    EXPECT_TRUE(editor.RunUntil([] { return LoadedFixtureBuild() == 1; }));
}

TEST(GameEditorLibrary, ARebuiltLibraryReloadsWithoutRestarting) {
    RUNIC_REQUIRE_LIBGODOT();
    ScratchProject project("reload");
    ASSERT_TRUE(project.Made());
    fs::create_directories(project.Path() / "Build/bin");
    fs::copy_file(RUNIC_FIXTURE_BUILD_1, project.Path() / kLibraryInProject);
    Simulation::SimulationRuntime world;
    EditorHarness editor(project.Path(), world);
    ASSERT_TRUE(editor.Start());
    ASSERT_TRUE(editor.RunUntil([] { return LoadedFixtureBuild() == 1; }));

    Replace(project.Path() / kLibraryInProject, RUNIC_FIXTURE_BUILD_2); // a build finished
    EXPECT_TRUE(editor.RunUntil([] { return LoadedFixtureBuild() == 2; }))
        << "the editor did not reload the rebuilt library";
}
