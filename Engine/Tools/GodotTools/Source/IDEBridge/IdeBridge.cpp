// The IDE bridge (IdeBridge.hpp).

#include "IdeBridge.hpp"

#include <algorithm>
#include <format>
#include <fstream>
#include <sstream>
#include <system_error>

#include "Project/Json.hpp"

namespace Runic::GodotTools {

namespace {

namespace fs = std::filesystem;
using Detail::JsonValue;
using Object = JsonValue::Object;
using Array = JsonValue::Array;

constexpr std::string_view kGoModFileName = "go.mod";
constexpr std::string_view kGoDirective = "go ";
constexpr std::string_view kTasksVersion = "2.0.0";
constexpr std::string_view kLaunchVersion = "0.2.0";
// VS Code's tasks.json and launch.json members, and what the tasks run.
constexpr std::string_view kLabelKey = "label";
constexpr std::string_view kTypeKey = "type";
constexpr std::string_view kShellType = "shell";
constexpr std::string_view kCommandKey = "command";
constexpr std::string_view kArgsKey = "args";
constexpr std::string_view kProblemMatcherKey = "problemMatcher";
constexpr std::string_view kGccMatcher = "$gcc";
constexpr std::string_view kGroupKey = "group";
constexpr std::string_view kBuildGroup = "build";
constexpr std::string_view kTestGroup = "test";
constexpr std::string_view kOptionsKey = "options";
constexpr std::string_view kCwdKey = "cwd";
constexpr std::string_view kWorkspaceFolder = "${workspaceFolder}";
constexpr std::string_view kVersionKey = "version";
constexpr std::string_view kTasksKey = "tasks";
constexpr std::string_view kConfigurationsKey = "configurations";
constexpr std::string_view kNameKey = "name";
constexpr std::string_view kRequestKey = "request";
constexpr std::string_view kLaunchRequest = "launch";
constexpr std::string_view kProgramKey = "program";
constexpr std::string_view kCppDbgType = "cppdbg";
constexpr std::string_view kMiModeKey = "MIMode";
constexpr std::string_view kGdbMode = "gdb";
constexpr std::string_view kPreLaunchTaskKey = "preLaunchTask";
constexpr std::string_view kConfigureLabel = "Runic: configure";
constexpr std::string_view kBuildLabel = "Runic: build";
constexpr std::string_view kTestLabel = "Runic: test";
constexpr std::string_view kGoTestLabel = "Runic: test Go services";
constexpr std::string_view kCMakeCommand = "cmake";
constexpr std::string_view kCTestCommand = "ctest";
constexpr std::string_view kGoCommand = "go";
constexpr std::string_view kPresetFlag = "--preset";
constexpr std::string_view kBuildFlag = "--build";
constexpr std::string_view kGoTestArgument = "test";
constexpr std::string_view kEveryPackage = "./...";
constexpr std::string_view kBinFolder = "bin"; // where the build puts executables
constexpr std::string_view kDebugLabelPrefix = "Runic: debug";
constexpr std::string_view kMarkerText =
    "Written by the RunicEditor (Project > Tools > Generate IDE files), which rewrites it";

Core::Result<void> WriteText(const fs::path &path, std::string_view text) {
    std::error_code error;
    fs::create_directories(path.parent_path(), error);
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) {
        return Core::Fail(std::format("cannot write {}", path.string()));
    }
    out.write(text.data(), static_cast<std::streamsize>(text.size()));
    return out ? Core::Result<void>{} : Core::Fail(std::format("cannot write {}", path.string()));
}

std::string ReadText(const fs::path &path) {
    std::ifstream in(path, std::ios::binary);
    std::ostringstream text;
    text << in.rdbuf();
    return text.str();
}

// Whether the bridge may write a VS Code file: it is absent, or it carries the bridge's marker.
bool IsOurs(const fs::path &path) {
    std::error_code error;
    if (!fs::exists(path, error)) {
        return true;
    }
    auto document = Detail::ParseJson(ReadText(path));
    return document && document.Value().IsObject() &&
           document.Value().Find(kGeneratedMarkerKey) != nullptr;
}

// "1.26.0" < "1.27" etc., by numeric parts.
bool OlderGo(const std::string &a, const std::string &b) {
    std::istringstream left(a);
    std::istringstream right(b);
    std::string x;
    std::string y;
    while (true) {
        const bool hasX = static_cast<bool>(std::getline(left, x, '.'));
        const bool hasY = static_cast<bool>(std::getline(right, y, '.'));
        if (!hasX || !hasY) {
            return !hasX && hasY;
        }
        const int u = std::atoi(x.c_str());
        const int v = std::atoi(y.c_str());
        if (u != v) {
            return u < v;
        }
    }
}

std::string GoWork(const RunicProject &project, std::vector<std::string> &modules) {
    std::string version(kDefaultGoVersion);
    bool declared = false;
    std::error_code error;
    const fs::path services = project.root / project.manifest.paths.services;
    if (fs::is_directory(services, error)) {
        for (const fs::directory_entry &entry : fs::directory_iterator(services, error)) {
            const fs::path goMod = entry.path() / kGoModFileName;
            if (!entry.is_directory() || !fs::is_regular_file(goMod, error)) {
                continue;
            }
            modules.push_back((fs::path(project.manifest.paths.services) / entry.path().filename())
                                  .generic_string());
            std::istringstream lines(ReadText(goMod));
            for (std::string line; std::getline(lines, line);) {
                if (line.starts_with(kGoDirective)) {
                    const std::string declaredVersion = line.substr(kGoDirective.size());
                    if (!declared || OlderGo(version, declaredVersion)) {
                        version = declaredVersion;
                    }
                    declared = true;
                }
            }
        }
    }
    std::sort(modules.begin(), modules.end());
    std::string text = std::format("go {}\n", version);
    if (!modules.empty()) {
        text += "\nuse (\n";
        for (const std::string &module : modules) {
            text += std::format("\t./{}\n", module);
        }
        text += ")\n";
    }
    return text;
}

JsonValue Text(std::string_view text) { return JsonValue(std::string(text)); }

JsonValue Task(std::string label, std::string command, Array args, std::string group) {
    Object task{{std::string(kLabelKey), JsonValue(std::move(label))},
                {std::string(kTypeKey), Text(kShellType)},
                {std::string(kCommandKey), JsonValue(std::move(command))},
                {std::string(kArgsKey), JsonValue(std::move(args))},
                {std::string(kProblemMatcherKey), JsonValue(Array{Text(kGccMatcher)})}};
    if (!group.empty()) {
        task.emplace_back(std::string(kGroupKey), JsonValue(std::move(group)));
    }
    return JsonValue(std::move(task));
}

std::string Tasks(const RunicProject &project, const IdeFilesSpec &spec, bool goServices) {
    const std::string preset = spec.configurePreset;
    Array tasks{
        Task(std::string(kConfigureLabel), std::string(kCMakeCommand),
             Array{Text(kPresetFlag), Text(preset)}, ""),
        Task(std::string(kBuildLabel), std::string(kCMakeCommand),
             Array{Text(kBuildFlag), Text(kPresetFlag), Text(preset)}, std::string(kBuildGroup)),
        Task(std::string(kTestLabel), std::string(kCTestCommand),
             Array{Text(kPresetFlag), Text(preset)}, std::string(kTestGroup)),
    };
    if (goServices) {
        Object task =
            Task(std::string(kGoTestLabel), std::string(kGoCommand),
                 Array{Text(kGoTestArgument), Text(kEveryPackage)}, std::string(kTestGroup))
                .AsObject();
        task.emplace_back(
            std::string(kOptionsKey),
            JsonValue(Object{
                {std::string(kCwdKey), JsonValue(std::format("{}/{}", kWorkspaceFolder,
                                                             project.manifest.paths.services))}}));
        tasks.push_back(JsonValue(std::move(task)));
    }
    Object root{{std::string(kGeneratedMarkerKey), Text(kMarkerText)},
                {std::string(kVersionKey), Text(kTasksVersion)},
                {std::string(kTasksKey), JsonValue(std::move(tasks))}};
    return Detail::WriteJson(JsonValue(std::move(root)));
}

std::string Launch(const RunicProject &project, const IdeFilesSpec &spec) {
    Array configurations;
    const auto add = [&](const std::string &executable) {
        if (executable.empty()) {
            return;
        }
        const std::string program =
            std::format("{}/{}/{}/{}", kWorkspaceFolder, spec.buildDirectory.generic_string(),
                        kBinFolder, executable);
        configurations.push_back(JsonValue(Object{
            {std::string(kNameKey), JsonValue(std::format("{} {}", kDebugLabelPrefix, executable))},
            {std::string(kTypeKey), Text(kCppDbgType)},
            {std::string(kRequestKey), Text(kLaunchRequest)},
            {std::string(kProgramKey), JsonValue(program)},
            {std::string(kArgsKey), JsonValue(Array{})},
            {std::string(kCwdKey), Text(kWorkspaceFolder)},
            {std::string(kMiModeKey), Text(kGdbMode)},
            {std::string(kPreLaunchTaskKey), Text(kBuildLabel)}}));
    };
    add(project.manifest.executables.server);
    add(project.manifest.executables.client);
    Object root{{std::string(kGeneratedMarkerKey), Text(kMarkerText)},
                {std::string(kVersionKey), Text(kLaunchVersion)},
                {std::string(kConfigurationsKey), JsonValue(std::move(configurations))}};
    return Detail::WriteJson(JsonValue(std::move(root)));
}

} // namespace

Core::Result<IdeFilesReport> WriteIdeFiles(const RunicProject &project, const IdeFilesSpec &spec) {
    IdeFilesReport report;
    std::error_code error;

    // clangd looks for compile_commands.json at the project's root: point there at CMake's.
    const fs::path commands = project.root / spec.buildDirectory / kCompileCommandsFileName;
    const fs::path link = project.root / kCompileCommandsFileName;
    if (!fs::is_regular_file(commands, error)) {
        report.notes.push_back(
            std::format("{} left alone: configure the project first ({} has none)",
                        kCompileCommandsFileName, spec.buildDirectory.generic_string()));
    } else if (fs::exists(fs::symlink_status(link, error)) && !fs::is_symlink(link, error)) {
        report.notes.push_back(
            std::format("{} left alone: it is not the bridge's link", kCompileCommandsFileName));
    } else {
        fs::remove(link, error);
        fs::create_symlink(fs::relative(commands, project.root, error), link, error);
        if (error) { // no symbolic links here: a copy, refreshed each time
            fs::copy_file(commands, link, fs::copy_options::overwrite_existing, error);
        }
        if (error) {
            return Core::Fail(std::format("cannot write {}: {}", link.string(), error.message()));
        }
        report.written.emplace_back(kCompileCommandsFileName);
    }

    std::vector<std::string> modules;
    const std::string goWork = GoWork(project, modules);
    if (auto written = WriteText(project.root / kGoWorkFileName, goWork); !written) {
        return Core::Fail(written.GetError().message);
    }
    report.written.emplace_back(kGoWorkFileName);

    const fs::path vscode = project.root / kVsCodeFolderName;
    const std::pair<std::string_view, std::string> files[] = {
        {kVsCodeTasksFileName, Tasks(project, spec, !modules.empty())},
        {kVsCodeLaunchFileName, Launch(project, spec)},
    };
    for (const auto &[name, text] : files) {
        const fs::path path = vscode / name;
        if (!IsOurs(path)) {
            report.notes.push_back(std::format("{}/{} left alone: it was not written by the bridge",
                                               kVsCodeFolderName, name));
            continue;
        }
        if (auto written = WriteText(path, text); !written) {
            return Core::Fail(written.GetError().message);
        }
        report.written.push_back(fs::path(kVsCodeFolderName) / name);
    }
    return report;
}

} // namespace Runic::GodotTools
