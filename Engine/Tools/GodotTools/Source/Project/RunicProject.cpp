// The project model (RunicProject.hpp): the manifest to and from JSON, opening a project, and New
// from the template.

#include "RunicProject.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <format>
#include <fstream>
#include <iterator>
#include <sstream>
#include <system_error>
#include <vector>

#include "Project/Json.hpp"

namespace Runic::GodotTools {

namespace {

using Detail::JsonValue;

// The manifest's member names (RunicDocs › Godot integration › Projects).
constexpr std::string_view kFormatKey = "runic";
constexpr std::string_view kEngineKey = "engine";
constexpr std::string_view kGameKey = "game";
constexpr std::string_view kNameKey = "name";
constexpr std::string_view kPathsKey = "paths";
constexpr std::string_view kContentKey = "content";
constexpr std::string_view kDataKey = "data";
constexpr std::string_view kSourceKey = "source";
constexpr std::string_view kServicesKey = "services";
constexpr std::string_view kCookedKey = "cooked";
constexpr std::string_view kExecutablesKey = "executables";
constexpr std::string_view kServerKey = "server";
constexpr std::string_view kClientKey = "client";
constexpr std::string_view kCookKey = "cook";
constexpr std::string_view kWorldKey = "world";
constexpr std::string_view kCellSizeKey = "cellSizeM";
constexpr std::string_view kClientPckKey = "clientPck";

// In the template's files, the game's name.
constexpr std::string_view kGameNamePlaceholder = "{{GAME_NAME}}";
// Files whose text New fills in; every other file is copied as bytes.
constexpr std::array<std::string_view, 6> kTextExtensions{".godot", ".gdextension", ".md",
                                                          ".txt",   ".json",        ".cmake"};
constexpr std::string_view kCMakeListsName = "CMakeLists.txt";
constexpr std::size_t kMaxGameNameLength = 64;

// Reads the members of one object of the manifest into the model, and reports anything wrong.
class Reader {
  public:
    explicit Reader(std::vector<std::string> &problems) : m_Problems(problems) {}

    // The object at key, or nullptr (and a problem when required).
    const JsonValue *Object(const JsonValue &parent, std::string_view key, std::string_view path,
                            bool required) {
        const JsonValue *value = parent.Find(key);
        if (value == nullptr) {
            if (required) {
                Problem(path, "is missing");
            }
            return nullptr;
        }
        if (!value->IsObject()) {
            Problem(path, "must be an object");
            return nullptr;
        }
        return value;
    }

    void Text(const JsonValue &parent, std::string_view key, std::string_view path,
              std::string &out, bool required) {
        const JsonValue *value = parent.Find(key);
        if (value == nullptr) {
            if (required) {
                Problem(path, "is missing");
            }
            return;
        }
        if (!value->IsString() || value->AsString().empty()) {
            Problem(path, "must be a non-empty string");
            return;
        }
        out = value->AsString();
    }

    void Integer(const JsonValue &parent, std::string_view key, std::string_view path,
                 std::int64_t &out, bool required) {
        const JsonValue *value = parent.Find(key);
        if (value == nullptr) {
            if (required) {
                Problem(path, "is missing");
            }
            return;
        }
        if (!value->IsInteger()) {
            Problem(path, "must be a whole number");
            return;
        }
        out = value->AsInteger();
    }

    // Members of an object that the model does not know.
    void Unknown(const JsonValue &object, std::string_view prefix,
                 std::initializer_list<std::string_view> known) {
        for (const auto &member : object.AsObject()) {
            if (std::find(known.begin(), known.end(), member.first) == known.end()) {
                Problem(std::format("{}{}", prefix, member.first), "is not a manifest member");
            }
        }
    }

    void Problem(std::string_view path, std::string_view what) {
        m_Problems.push_back(std::format("{}: {}", path, what));
    }

  private:
    std::vector<std::string> &m_Problems;
};

std::string Joined(const std::vector<std::string> &problems) {
    std::string text = std::format("{} problem(s) in {}", problems.size(), kManifestFileName);
    for (const std::string &problem : problems) {
        text += "\n  " + problem;
    }
    return text;
}

Core::Result<std::string> ReadFile(const std::filesystem::path &path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return Core::Fail(std::format("cannot read {}", path.string()));
    }
    std::ostringstream text;
    text << in.rdbuf();
    return text.str();
}

Core::Result<void> WriteFile(const std::filesystem::path &path, std::string_view text) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) {
        return Core::Fail(std::format("cannot write {}", path.string()));
    }
    out.write(text.data(), static_cast<std::streamsize>(text.size()));
    if (!out) {
        return Core::Fail(std::format("cannot write {}", path.string()));
    }
    return {};
}

bool IsTextFile(const std::filesystem::path &path) {
    if (path.filename() == kCMakeListsName) {
        return true;
    }
    const std::string extension = path.extension().string();
    return std::find(kTextExtensions.begin(), kTextExtensions.end(), extension) !=
           kTextExtensions.end();
}

std::string Filled(std::string text, std::string_view gameName) {
    for (std::size_t at = text.find(kGameNamePlaceholder); at != std::string::npos;
         at = text.find(kGameNamePlaceholder, at + gameName.size())) {
        text.replace(at, kGameNamePlaceholder.size(), gameName);
    }
    return text;
}

bool IsValidGameName(std::string_view name) {
    if (name.empty() || name.size() > kMaxGameNameLength || name.front() == ' ' ||
        name.back() == ' ') {
        return false;
    }
    return std::all_of(name.begin(), name.end(), [](char c) {
        return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == ' ' || c == '-' || c == '_';
    });
}

} // namespace

Core::Result<ProjectManifest> ParseProjectManifest(std::string_view text) {
    auto document = Detail::ParseJson(text);
    if (!document) {
        return Core::Fail(std::format("{} is not valid JSON: {}", kManifestFileName,
                                      document.GetError().message));
    }
    const JsonValue &root = document.Value();
    if (!root.IsObject()) {
        return Core::Fail(std::format("{} must hold one object", kManifestFileName));
    }

    ProjectManifest manifest;
    std::vector<std::string> problems;
    Reader read(problems);
    read.Integer(root, kFormatKey, kFormatKey, manifest.format, true);
    if (problems.empty() && manifest.format != kManifestFormat) {
        read.Problem(kFormatKey, std::format("format {} is not the one this Engine reads ({})",
                                             manifest.format, kManifestFormat));
    }
    read.Text(root, kEngineKey, kEngineKey, manifest.engine, true);
    if (const JsonValue *game = read.Object(root, kGameKey, kGameKey, true)) {
        read.Text(*game, kNameKey, "game.name", manifest.gameName, true);
        read.Unknown(*game, "game.", {kNameKey});
    }
    if (const JsonValue *paths = read.Object(root, kPathsKey, kPathsKey, false)) {
        read.Text(*paths, kContentKey, "paths.content", manifest.paths.content, false);
        read.Text(*paths, kDataKey, "paths.data", manifest.paths.data, false);
        read.Text(*paths, kSourceKey, "paths.source", manifest.paths.source, false);
        read.Text(*paths, kServicesKey, "paths.services", manifest.paths.services, false);
        read.Text(*paths, kCookedKey, "paths.cooked", manifest.paths.cooked, false);
        read.Unknown(*paths, "paths.",
                     {kContentKey, kDataKey, kSourceKey, kServicesKey, kCookedKey});
    }
    if (const JsonValue *executables = read.Object(root, kExecutablesKey, kExecutablesKey, false)) {
        read.Text(*executables, kServerKey, "executables.server", manifest.executables.server,
                  false);
        read.Text(*executables, kClientKey, "executables.client", manifest.executables.client,
                  false);
        read.Unknown(*executables, "executables.", {kServerKey, kClientKey});
    }
    if (const JsonValue *cook = read.Object(root, kCookKey, kCookKey, false)) {
        read.Text(*cook, kWorldKey, "cook.world", manifest.cook.world, false);
        read.Integer(*cook, kCellSizeKey, "cook.cellSizeM", manifest.cook.cellSizeM, false);
        if (manifest.cook.cellSizeM <= 0) {
            read.Problem("cook.cellSizeM", "must be more than 0");
        }
        read.Text(*cook, kClientPckKey, "cook.clientPck", manifest.cook.clientPck, false);
        read.Unknown(*cook, "cook.", {kWorldKey, kCellSizeKey, kClientPckKey});
    }
    read.Unknown(root, "",
                 {kFormatKey, kEngineKey, kGameKey, kPathsKey, kExecutablesKey, kCookKey});
    if (!problems.empty()) {
        return Core::Fail(Joined(problems));
    }
    return manifest;
}

std::string WriteProjectManifest(const ProjectManifest &manifest) {
    using Object = JsonValue::Object;
    auto text = [](const std::string &value) { return JsonValue(value); };
    Object game{{std::string(kNameKey), text(manifest.gameName)}};
    Object paths{{std::string(kContentKey), text(manifest.paths.content)},
                 {std::string(kDataKey), text(manifest.paths.data)},
                 {std::string(kSourceKey), text(manifest.paths.source)},
                 {std::string(kServicesKey), text(manifest.paths.services)},
                 {std::string(kCookedKey), text(manifest.paths.cooked)}};
    Object executables;
    if (!manifest.executables.server.empty()) {
        executables.emplace_back(std::string(kServerKey), text(manifest.executables.server));
    }
    if (!manifest.executables.client.empty()) {
        executables.emplace_back(std::string(kClientKey), text(manifest.executables.client));
    }
    Object cook{{std::string(kWorldKey), text(manifest.cook.world)},
                {std::string(kCellSizeKey), JsonValue(manifest.cook.cellSizeM)},
                {std::string(kClientPckKey), text(manifest.cook.clientPck)}};
    Object root{{std::string(kFormatKey), JsonValue(manifest.format)},
                {std::string(kEngineKey), text(manifest.engine)},
                {std::string(kGameKey), JsonValue(std::move(game))},
                {std::string(kPathsKey), JsonValue(std::move(paths))},
                {std::string(kExecutablesKey), JsonValue(std::move(executables))},
                {std::string(kCookKey), JsonValue(std::move(cook))}};
    return Detail::WriteJson(JsonValue(std::move(root)));
}

bool IsRunicProject(const std::filesystem::path &root) {
    std::error_code error;
    return std::filesystem::is_regular_file(root / kManifestFileName, error);
}

Core::Result<RunicProject> OpenProject(const std::filesystem::path &root) {
    auto text = ReadFile(root / kManifestFileName);
    if (!text) {
        return Core::Fail(
            std::format("{} is not a Runic project: {}", root.string(), text.GetError().message));
    }
    auto manifest = ParseProjectManifest(text.Value());
    if (!manifest) {
        return Core::Fail(manifest.GetError().message);
    }
    return RunicProject{root, manifest.Value()};
}

Core::Result<RunicProject> InitializeProject(const NewProjectSpec &spec) {
    namespace fs = std::filesystem;
    std::error_code error;
    if (!IsValidGameName(spec.gameName)) {
        return Core::Fail(std::format("\"{}\" is not a game name: use letters, digits, spaces, '-' "
                                      "and '_', at most {} characters",
                                      spec.gameName, kMaxGameNameLength));
    }
    if (!fs::is_directory(spec.templateDirectory, error)) {
        return Core::Fail(
            std::format("the template {} does not exist", spec.templateDirectory.string()));
    }
    fs::create_directories(spec.root, error);
    if (error) {
        return Core::Fail(std::format("cannot create {}: {}", spec.root.string(), error.message()));
    }
    // Only what Godot's project manager makes may already be there.
    for (const fs::directory_entry &entry : fs::directory_iterator(spec.root, error)) {
        const std::string name = entry.path().filename().string();
        if (name != kGodotProjectFileName && name != kGodotCacheFolderName) {
            return Core::Fail(std::format("{} is not empty ({} is there): New makes a project only "
                                          "in an empty folder",
                                          spec.root.string(), name));
        }
    }

    for (auto it = fs::recursive_directory_iterator(spec.templateDirectory, error);
         it != fs::recursive_directory_iterator(); it.increment(error)) {
        if (error) {
            return Core::Fail(std::format("cannot read the template: {}", error.message()));
        }
        const fs::path relative = fs::relative(it->path(), spec.templateDirectory, error);
        const fs::path target = spec.root / relative;
        if (it->is_directory()) {
            fs::create_directories(target, error);
            continue;
        }
        if (relative == kGodotProjectFileName && fs::exists(target, error)) {
            continue; // the project manager's own project.godot stays: the editor has it open
        }
        if (!IsTextFile(it->path())) {
            fs::copy_file(it->path(), target, fs::copy_options::overwrite_existing, error);
            if (error) {
                return Core::Fail(
                    std::format("cannot copy {}: {}", relative.string(), error.message()));
            }
            continue;
        }
        auto text = ReadFile(it->path());
        if (!text) {
            return Core::Fail(text.GetError().message);
        }
        if (auto written = WriteFile(target, Filled(text.Value(), spec.gameName)); !written) {
            return Core::Fail(written.GetError().message);
        }
    }

    ProjectManifest manifest;
    manifest.engine = spec.engineVersion;
    manifest.gameName = spec.gameName;
    if (auto written = WriteFile(spec.root / kManifestFileName, WriteProjectManifest(manifest));
        !written) {
        return Core::Fail(written.GetError().message);
    }
    return RunicProject{spec.root, manifest};
}

} // namespace Runic::GodotTools
