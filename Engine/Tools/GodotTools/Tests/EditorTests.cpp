// GDT-2 with Godot: the RunicEditor's tools in the editor library, headless, in this process. The
// editor opens a project made from the template, loads its editor library, and reloads it when a
// build replaces the file. Each test starts its own Godot: LibGodot allows one instance per
// process, and CTest runs every test in its own process.

#include "ApplicationHost.hpp"
#include "GodotLayer.hpp"
#include "GodotRuntime.hpp"
#include "GodotToolsModule.hpp"
#include "RunicProject.hpp"
#include "SimulationRuntime.hpp"

#include <godot_cpp/classes/class_db_singleton.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/window.hpp>

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <utility>

using namespace Runic;
namespace fs = std::filesystem;

namespace {

constexpr const char *kTestExecutable = "/RunicGodotToolsTests";
constexpr const char *kFixtureClass = "RunicFixtureNode";
constexpr const char *kBuildIdMethod = "build_id";
constexpr const char *kLibraryInProject = "Build/bin/libgame_editor.so";
// How long the editor may take to load or reload the library: its first scan, then two looks of the
// watcher half a second apart.
constexpr auto kPatience = std::chrono::seconds(60);
constexpr auto kFramePause = std::chrono::milliseconds(5);
// After a load or a reload the editor regenerates the extension's documentation in a deferred call; a
// second of frames lets it run before the test shuts the editor down.
constexpr auto kSettle = std::chrono::seconds(1);

bool LibGodotBuilt() {
    return fs::exists(
        GodotBridge::GodotRuntime::DefaultLibraryPath(GodotBridge::GodotTarget::Editor));
}

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

class ScratchProject {
  public:
    explicit ScratchProject(std::string_view name)
        : m_Path(fs::temp_directory_path() / ("runic-gdt2-editor-" + std::string(name))) {
        fs::remove_all(m_Path);
        GodotTools::NewProjectSpec spec;
        spec.templateDirectory = fs::path(RUNIC_TEMPLATES_DIR) / GodotTools::kDefaultTemplateName;
        spec.root = m_Path;
        spec.gameName = "Fixture Game";
        spec.engineVersion = "0.1.0";
        m_Made = static_cast<bool>(GodotTools::InitializeProject(spec));
    }
    ~ScratchProject() {
        std::error_code ignored;
        fs::remove_all(m_Path, ignored);
    }
    bool Made() const { return m_Made; }
    const fs::path &Path() const { return m_Path; }

  private:
    fs::path m_Path;
    bool m_Made = false;
};

// The RunicEditor's composition, headless, on a project: one GodotLayer with the editor tools
// registered.
class EditorHarness {
  public:
    EditorHarness(const fs::path &project, Simulation::SimulationRuntime &world) {
        GodotBridge::GodotLayerSpec spec;
        spec.arguments = {GodotBridge::GodotRuntime::ExecutableDirectory() + kTestExecutable,
                          "--headless", "--editor", "--path", project.string()};
        spec.forwardInput = false;
        auto layer = std::make_unique<GodotBridge::GodotLayer>(std::move(spec));
        GodotTools::EditorToolsSpec tools;
        tools.localWorld = &world;
        tools.templateDirectory = fs::path(RUNIC_TEMPLATES_DIR) / GodotTools::kDefaultTemplateName;
        tools.engineVersion = "0.1.0";
        GodotTools::RegisterEditorTools(layer->Runtime(), tools);
        auto application = std::make_unique<Application::Application>(
            Application::ApplicationSpec{"test", Application::LoopPolicy::PacedByTopLayer(), 0});
        application->PushLayer(std::move(layer));
        m_Host = std::make_unique<Application::ApplicationHost>(
            std::move(application), Application::CommandLine({kTestExecutable}));
    }
    ~EditorHarness() {
        RunFor(kSettle);
        m_Host->Shutdown();
    }
    EditorHarness(const EditorHarness &) = delete;
    EditorHarness &operator=(const EditorHarness &) = delete;

    bool Start() { return static_cast<bool>(m_Host->Start()); }
    // Runs editor frames until done() or the patience runs out.
    bool RunUntil(const std::function<bool()> &done) {
        const auto deadline = std::chrono::steady_clock::now() + kPatience;
        while (std::chrono::steady_clock::now() < deadline) {
            m_Host->RunFrame(Application::FrameTime{m_Host->FramesRun()});
            if (done()) {
                return true;
            }
            std::this_thread::sleep_for(kFramePause);
        }
        return false;
    }

  private:
    void RunFor(std::chrono::steady_clock::duration time) {
        const auto deadline = std::chrono::steady_clock::now() + time;
        while (std::chrono::steady_clock::now() < deadline) {
            m_Host->RunFrame(Application::FrameTime{m_Host->FramesRun()});
            std::this_thread::sleep_for(kFramePause);
        }
    }

    std::unique_ptr<Application::ApplicationHost> m_Host;
};

} // namespace

#define RUNIC_REQUIRE_LIBGODOT()                                                                   \
    if (!LibGodotBuilt()) {                                                                        \
        GTEST_SKIP() << "libgodot is not built";                                                   \
    }

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
