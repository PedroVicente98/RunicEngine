#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>

#include "Result.hpp"

// The project model (GDT-2; RunicDocs › Godot integration (Engine) › Projects: create, open and the
// game's editor library). A Runic project is a Godot project (project.godot) with a Runic manifest
// beside it, runic.project.json: the Engine it was made for, its game's name, where its content,
// data, source, services and cooked output live, its executables and its cook settings. The
// RunicEditor opens any Godot project; a project with a manifest is a Runic project, and an empty
// one can be made into one from the runic-mmo template (New, through a plugin: open decision 2).

namespace Runic::GodotTools {

inline constexpr std::string_view kManifestFileName = "runic.project.json";
inline constexpr std::string_view kGodotProjectFileName = "project.godot";
inline constexpr std::string_view kGameEditorExtensionFileName = "game_editor.gdextension";
// The folder Godot keeps its import cache in; an empty project may already have it.
inline constexpr std::string_view kGodotCacheFolderName = ".godot";
// The manifest format this Engine reads and writes ("runic": 1).
inline constexpr std::int64_t kManifestFormat = 1;
// The template a new project is made from, under the Engine's templates folder.
inline constexpr std::string_view kDefaultTemplateName = "runic-mmo";

struct ProjectPaths {
    std::string content = "Content";
    std::string data = "Data";
    std::string source = "Source";
    std::string services = "Services";
    std::string cooked = "Build/Cooked";

    friend bool operator==(const ProjectPaths &, const ProjectPaths &) = default;
};

struct ProjectExecutables {
    std::string server;
    std::string client;

    friend bool operator==(const ProjectExecutables &, const ProjectExecutables &) = default;
};

struct CookSettings {
    std::string world = "Content/World.tscn";
    std::int64_t cellSizeM = 64;
    std::string clientPck = "Build/Client/game.pck";

    friend bool operator==(const CookSettings &, const CookSettings &) = default;
};

struct ProjectManifest {
    std::int64_t format = kManifestFormat;
    std::string engine; // the Engine version the project was made with, such as "0.1.0"
    std::string gameName;
    ProjectPaths paths;
    ProjectExecutables executables;
    CookSettings cook;

    friend bool operator==(const ProjectManifest &, const ProjectManifest &) = default;
};

// Reads a manifest. Every problem names the member it is about ("paths.content: ..."); members the
// Engine does not know are kept out of the model and reported, so a typo is not silently ignored.
Core::Result<ProjectManifest> ParseProjectManifest(std::string_view text);
// Writes a manifest in the layout RunicDocs shows.
std::string WriteProjectManifest(const ProjectManifest &manifest);

// A project on disk: its folder and its manifest.
struct RunicProject {
    std::filesystem::path root;
    ProjectManifest manifest;
};

// Whether a folder holds a manifest.
bool IsRunicProject(const std::filesystem::path &root);
// Opens the project in root: reads and checks its manifest.
Core::Result<RunicProject> OpenProject(const std::filesystem::path &root);

struct NewProjectSpec {
    std::filesystem::path
        templateDirectory;      // the template's folder, such as <Engine>/templates/runic-mmo
    std::filesystem::path root; // the project's folder
    std::string gameName;       // letters, digits, spaces, '-' and '_'
    std::string engineVersion;
};

// New: makes root a Runic project from the template. root may hold Godot's own project.godot and
// .godot folder (a project the project manager just created, open in the editor) and nothing else;
// the template's files are copied in (an existing project.godot is kept), {{GAME_NAME}} in them
// becomes the game's name, and the manifest is written. Nothing is written when root holds
// anything else.
Core::Result<RunicProject> InitializeProject(const NewProjectSpec &spec);

} // namespace Runic::GodotTools
