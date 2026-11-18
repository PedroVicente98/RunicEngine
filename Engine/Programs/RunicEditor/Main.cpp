// RunicEditor: the Godot editor run through LibGodot, with the Runic tools compiled in
// (GodotTools), on the Engine's entry point since GDB-1 (slice 1). Its stack, from slice 6: the
// Simulation layer with the editor's local world (what the Runic World dock inspects), then the
// GodotLayer (editor library, no input forwarding).
//
//   RunicEditor
//       Godot's project manager (New and Open; Project > Tools > Runic: make this a Runic project
//       turns a new, empty Godot project into a Runic one)
//   RunicEditor --editor --path <project>
//       opens a project, as the project manager does
//   RunicEditor --runic-new-project <folder> <game name>
//       makes <folder> a Runic project from the runic-mmo template and exits, without Godot
//   RunicEditor --headless --editor --path <project> --quit-after 120
//       the CI check: open the project, load the plugin, show the dock, exit
//   RunicEditor --headless --editor --path <project> --runic-cook [--runic-cook-scene=<res
//   path>...]
//       cooks once the project is imported (the manifest's world scene, or the scenes named),
//       prints the report and exits: 0 when the cook succeeded (GDT-4)
//
// Every argument goes to Godot unchanged: the project manager opens a project by relaunching this
// executable with --path, so the entry point never consumes or rewrites arguments.

#include "Application.hpp"
#include "EntryPoint.hpp"
#include "GodotLayer.hpp"
#include "GodotToolsModule.hpp"
#include "LocalWorld.hpp"
#include "Log.hpp"
#include "RunicProject.hpp"
#include "SimulationLayer.hpp"
#include "SimulationRuntime.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

namespace {

using Runic::Application::Application;
using Runic::Application::ApplicationSpec;
using Runic::Application::CommandLine;
using Runic::Application::kCleanExit;
using Runic::Application::LoopPolicy;

constexpr const char *kProgramName = "RunicEditor";
// RunicEditor's own command: New without Godot. It is handled before Godot starts.
constexpr std::string_view kNewProjectFlag = "--runic-new-project";
constexpr int kNewProjectArguments = 2; // the folder and the game's name
constexpr std::string_view kNewProjectLayerName = "engine:new_project";
// --runic-cook, and the scenes it cooks (one flag each, as a value Godot does not read as a scene
// to open).
constexpr std::string_view kCookFlag = "--runic-cook";
constexpr std::string_view kCookSceneFlag = "--runic-cook-scene=";
constexpr std::string_view kExitCodeLayerName = "engine:exit_code";
constexpr std::uint64_t kOneFrame = 1;

#ifndef RUNIC_TEMPLATES_DIR
#define RUNIC_TEMPLATES_DIR "templates"
#endif
#ifndef RUNIC_ENGINE_VERSION
#define RUNIC_ENGINE_VERSION "0.0.0"
#endif

// Where the runic-mmo template is: staged by the build next to the executables' folder.
std::filesystem::path TemplateDirectory() {
    return std::filesystem::path(RUNIC_TEMPLATES_DIR) / Runic::GodotTools::kDefaultTemplateName;
}

// --runic-new-project <folder> <game name>: makes the project and stops, with Godot never started.
// A failure fails the attach, so the program exits with the host's code for it.
class NewProjectLayer final : public Runic::Application::Layer {
  public:
    explicit NewProjectLayer(Runic::GodotTools::NewProjectSpec project)
        : Layer(std::string(kNewProjectLayerName)), m_Project(std::move(project)) {}

    Runic::Core::Result<void> OnAttach(Runic::Application::LayerContext &context) override {
        auto made = Runic::GodotTools::InitializeProject(m_Project);
        if (!made) {
            return Runic::Core::Fail("no new project: " + made.GetError().message);
        }
        RUNIC_LOG_INFO("RUNIC: new Runic project {} in {}", m_Project.gameName,
                       m_Project.root.string());
        context.RequestStop(kCleanExit);
        return {};
    }

  private:
    Runic::GodotTools::NewProjectSpec m_Project;
};

// Godot's Play buttons relaunch the editor executable without --editor and with --editor-pid to run
// the project as a game. In a Runic project the game runs through Runic's Run (GDT-9, slice 13),
// never inside Godot; until then this safety net stops the relaunch.
bool IsGodotPlayRequest(const CommandLine &command) {
    return command.Has(Runic::GodotBridge::kEditorPidFlag) &&
           !command.Has(Runic::GodotBridge::kEditorFlag) &&
           !command.Has(Runic::GodotBridge::kEditorShortFlag);
}

class RunicEditorApplication final : public Application {
  public:
    using Application::Application;

    void OnStopped(int exitCode, std::uint64_t frames) override {
        if (exitCode == kCleanExit) {
            RUNIC_LOG_INFO("RUNIC: {} exited cleanly after {} frames", Spec().name, frames);
        } else {
            RUNIC_LOG_ERROR("RUNIC: {} exited with code {} after {} frames", Spec().name, exitCode,
                            frames);
        }
    }
};

// Stops the editor with an exit code the tools give it (--runic-cook's), before Godot's quit stops
// it as a clean exit: the first stop request wins.
class ExitCodeLayer final : public Runic::Application::Layer {
  public:
    ExitCodeLayer() : Layer(std::string(kExitCodeLayerName)) {}
    Runic::Core::Result<void> OnAttach(Runic::Application::LayerContext &context) override {
        m_Context = &context;
        return {};
    }
    void Stop(int code) {
        if (m_Context != nullptr) {
            m_Context->RequestStop(code);
        }
    }

  private:
    Runic::Application::LayerContext *m_Context = nullptr;
};

} // namespace

std::unique_ptr<Application> Runic::Application::CreateApplication(const CommandLine &command) {
    if (IsGodotPlayRequest(command)) {
        RUNIC_LOG_WARNING("RUNIC: Godot's Play runs the project inside Godot; a Runic project runs "
                          "through Runic's Run instead (not available before slice 13).");
        return nullptr;
    }
    const auto &arguments = command.Arguments();
    for (std::size_t i = 0; i < arguments.size(); ++i) {
        if (arguments[i] != kNewProjectFlag) {
            continue;
        }
        if (i + kNewProjectArguments >= arguments.size()) {
            RUNIC_LOG_ERROR("RUNIC: {} needs a folder and a game name", kNewProjectFlag);
            return nullptr;
        }
        GodotTools::NewProjectSpec project;
        project.templateDirectory = TemplateDirectory();
        project.root = arguments[i + 1];
        project.gameName = arguments[i + 2];
        project.engineVersion = RUNIC_ENGINE_VERSION;
        auto application = std::make_unique<RunicEditorApplication>(
            ApplicationSpec{kProgramName, LoopPolicy::PacedByTopLayer(), kOneFrame});
        application->PushLayer(std::make_unique<NewProjectLayer>(std::move(project)));
        return application;
    }

    // The editor's local world: what the Runic World dock inspects.
    auto simulation = std::make_unique<Simulation::SimulationLayer>(
        std::make_unique<Simulation::SimulationRuntime>());
    Programs::PopulateLocalWorld(simulation->Runtime());

    GodotBridge::GodotLayerSpec spec;
    spec.target = GodotBridge::GodotTarget::Editor;
    spec.arguments = command.Arguments();
    spec.forwardInput = false; // the editor's input belongs to the editor
    auto layer = std::make_unique<GodotBridge::GodotLayer>(std::move(spec));
    GodotTools::EditorToolsSpec tools;
    tools.localWorld = &simulation->Runtime();
    tools.templateDirectory = TemplateDirectory();
    tools.engineVersion = RUNIC_ENGINE_VERSION;
    auto exitCode = std::make_unique<ExitCodeLayer>();
    ExitCodeLayer *stopper = exitCode.get();
    tools.cookFinished = [stopper](int code) { stopper->Stop(code); };
    for (const std::string &argument : arguments) {
        if (argument == kCookFlag) {
            tools.cookAndQuit = true;
        } else if (argument.starts_with(kCookSceneFlag)) {
            tools.cookAndQuit = true;
            tools.cookScenes.push_back(argument.substr(kCookSceneFlag.size()));
        }
    }
    GodotTools::RegisterEditorTools(layer->Runtime(), tools);

    // The editor paces its own frames, and Godot reads the frame limit (kQuitAfterFlag) itself.
    auto application = std::make_unique<RunicEditorApplication>(ApplicationSpec{
        kProgramName, LoopPolicy::PacedByTopLayer(), ApplicationSpec::kRunUntilStopped});
    application->PushLayer(std::move(simulation));
    application->PushLayer(std::move(exitCode));
    application->PushLayer(std::move(layer));
    return application;
}

// The Engine owns main(): it is defined here, once, and hands over to the Engine.
RUNIC_ENTRY_POINT()
