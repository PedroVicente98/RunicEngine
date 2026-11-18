#pragma once

// The RunicEditor's composition for GodotTools' tests with Godot: a scratch project made from the
// template, and an editor on it, headless, in the test process. Each test starts its own Godot:
// LibGodot allows one instance per process, and CTest runs every test in its own process.

#include "ApplicationHost.hpp"
#include "GodotLayer.hpp"
#include "GodotRuntime.hpp"
#include "GodotToolsModule.hpp"
#include "RunicProject.hpp"
#include "SimulationRuntime.hpp"
#include "WorldSpace.hpp"

#include <godot_cpp/classes/editor_interface.hpp>
#include <godot_cpp/classes/script_editor.hpp>

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <utility>

namespace Runic::GodotTools::Testing {

namespace fs = std::filesystem;

inline constexpr const char *kTestExecutable = "/RunicGodotToolsTests";
// How long the editor may take to load or reload the library: its first scan, then two looks of the
// watcher half a second apart.
inline constexpr auto kPatience = std::chrono::seconds(60);
inline constexpr auto kFramePause = std::chrono::milliseconds(5);
// After a load or a reload the editor regenerates the extension's documentation in a deferred call;
// a second of frames lets it run before the test shuts the editor down.
inline constexpr auto kSettle = std::chrono::seconds(1);
// Godot 4.7.2 loads its help cache on a thread that ends by queuing the extension docs' generation;
// shutting the editor down before that call ran crashes in Main::cleanup (the docs are gone by
// then). Opening a help page waits for that thread, so the call is queued before the settle frames
// run.
inline constexpr const char *kAnyHelpTopic = "class_name:Object";

inline bool LibGodotBuilt() {
    return fs::exists(
        GodotBridge::GodotRuntime::DefaultLibraryPath(GodotBridge::GodotTarget::Editor));
}

class ScratchProject {
  public:
    explicit ScratchProject(std::string_view name)
        : m_Path(fs::temp_directory_path() / ("runic-editor-test-" + std::string(name))) {
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
    EditorHarness(const fs::path &project, Simulation::SimulationRuntime &world,
                  Core::WorldPosition sceneOrigin = {}) {
        GodotBridge::GodotLayerSpec spec;
        spec.arguments = {GodotBridge::GodotRuntime::ExecutableDirectory() + kTestExecutable,
                          "--headless", "--editor", "--path", project.string()};
        spec.forwardInput = false;
        auto layer = std::make_unique<GodotBridge::GodotLayer>(std::move(spec));
        GodotTools::EditorToolsSpec tools;
        tools.localWorld = &world;
        tools.editedSceneOrigin = sceneOrigin;
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
        if (m_Started) {
            WaitForEditorHelp();
        }
        RunFor(kSettle);
        m_Host->Shutdown();
    }
    EditorHarness(const EditorHarness &) = delete;
    EditorHarness &operator=(const EditorHarness &) = delete;

    bool Start() {
        m_Started = static_cast<bool>(m_Host->Start());
        return m_Started;
    }
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
    static void WaitForEditorHelp() {
        godot::EditorInterface *editor = godot::EditorInterface::get_singleton();
        if (editor == nullptr || editor->get_script_editor() == nullptr) {
            return;
        }
        editor->get_script_editor()->goto_help(kAnyHelpTopic);
    }

    void RunFor(std::chrono::steady_clock::duration time) {
        const auto deadline = std::chrono::steady_clock::now() + time;
        while (std::chrono::steady_clock::now() < deadline) {
            m_Host->RunFrame(Application::FrameTime{m_Host->FramesRun()});
            std::this_thread::sleep_for(kFramePause);
        }
    }

    std::unique_ptr<Application::ApplicationHost> m_Host;
    bool m_Started = false;
};

} // namespace Runic::GodotTools::Testing

#define RUNIC_REQUIRE_LIBGODOT()                                                                   \
    if (!Runic::GodotTools::Testing::LibGodotBuilt()) {                                            \
        GTEST_SKIP() << "libgodot is not built";                                                   \
    }
