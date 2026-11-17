#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "Result.hpp"
#include "RunicProject.hpp"

// The IDE bridge (GDT-2; RunicDocs › Godot integration › Game code and the Run button › External
// IDE and assistants): C++ and Go are written in an external IDE, and the editor writes what the
// IDE needs into the project: compile_commands.json for clangd, go.work over the project's Go
// services, and VS Code's tasks and launch configurations. A file the bridge did not write is never
// replaced.

namespace Runic::GodotTools {

inline constexpr std::string_view kCompileCommandsFileName = "compile_commands.json";
inline constexpr std::string_view kGoWorkFileName = "go.work";
inline constexpr std::string_view kVsCodeFolderName = ".vscode";
inline constexpr std::string_view kVsCodeTasksFileName = "tasks.json";
inline constexpr std::string_view kVsCodeLaunchFileName = "launch.json";
// The member that marks a VS Code file as the bridge's: the bridge rewrites only files that have
// it.
inline constexpr std::string_view kGeneratedMarkerKey = "runicGenerated";
// The Go version go.work names when no service declares one: the Platform's toolchain.
inline constexpr std::string_view kDefaultGoVersion = "1.26.0";

struct IdeFilesSpec {
    // The CMake configure preset the tasks use, and the build folder it configures (relative to
    // the project's folder), where compile_commands.json and the executables are.
    std::string configurePreset = "debug";
    std::filesystem::path buildDirectory = "Build/debug";
};

struct IdeFilesReport {
    std::vector<std::filesystem::path> written; // relative to the project's folder
    std::vector<std::string> notes;             // what was left alone, and why
};

// Writes the IDE files of a project. Fails only when a file cannot be written.
Core::Result<IdeFilesReport> WriteIdeFiles(const RunicProject &project, const IdeFilesSpec &spec);

} // namespace Runic::GodotTools
