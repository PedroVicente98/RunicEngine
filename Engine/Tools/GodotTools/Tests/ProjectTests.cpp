// GDT-2: the project model (runic.project.json and New) and the IDE bridge, without Godot.

#include "IdeBridge.hpp"
#include "RunicProject.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

using namespace Runic;
using namespace Runic::GodotTools;
namespace fs = std::filesystem;

namespace {

// RunicDocs' example manifest (Godot integration › Projects), with the Engine version this one has.
constexpr std::string_view kDocsManifest = R"({
  "runic": 1,
  "engine": "0.4.0",
  "game": { "name": "RunicGame" },
  "paths": { "content": "Content", "data": "Data", "source": "Source",
             "services": "Services", "cooked": "Build/Cooked" },
  "executables": { "server": "RunicGameServer", "client": "RunicGameClient" },
  "cook": { "world": "Content/World.tscn", "cellSizeM": 64, "clientPck": "Build/Client/game.pck" }
})";

const fs::path kTemplate = fs::path(RUNIC_TEMPLATES_DIR) / kDefaultTemplateName;

std::string ReadAll(const fs::path &path) {
    std::ifstream in(path, std::ios::binary);
    std::ostringstream text;
    text << in.rdbuf();
    return text.str();
}

void WriteAll(const fs::path &path, std::string_view text) {
    fs::create_directories(path.parent_path());
    std::ofstream(path, std::ios::binary) << text;
}

// A fresh folder for one test, removed when it ends.
class ScratchFolder {
  public:
    explicit ScratchFolder(std::string_view name)
        : m_Path(fs::temp_directory_path() / ("runic-gdt2-" + std::string(name))) {
        fs::remove_all(m_Path);
        fs::create_directories(m_Path);
    }
    ~ScratchFolder() {
        std::error_code ignored;
        fs::remove_all(m_Path, ignored);
    }
    const fs::path &Path() const { return m_Path; }

  private:
    fs::path m_Path;
};

RunicProject NewProject(const fs::path &root, std::string_view name = "Test Game") {
    NewProjectSpec spec;
    spec.templateDirectory = kTemplate;
    spec.root = root;
    spec.gameName = name;
    spec.engineVersion = "0.1.0";
    auto project = InitializeProject(spec);
    EXPECT_TRUE(project) << project.GetError().message;
    return project ? project.Value() : RunicProject{};
}

} // namespace

TEST(ProjectManifest, ReadsTheManifestRunicDocsShows) {
    const auto manifest = ParseProjectManifest(kDocsManifest);
    ASSERT_TRUE(manifest) << manifest.GetError().message;
    EXPECT_EQ(manifest.Value().format, kManifestFormat);
    EXPECT_EQ(manifest.Value().engine, "0.4.0");
    EXPECT_EQ(manifest.Value().gameName, "RunicGame");
    EXPECT_EQ(manifest.Value().paths.cooked, "Build/Cooked");
    EXPECT_EQ(manifest.Value().executables.server, "RunicGameServer");
    EXPECT_EQ(manifest.Value().executables.client, "RunicGameClient");
    EXPECT_EQ(manifest.Value().cook.world, "Content/World.tscn");
    EXPECT_EQ(manifest.Value().cook.cellSizeM, 64);
    EXPECT_EQ(manifest.Value().cook.clientPck, "Build/Client/game.pck");
}

TEST(ProjectManifest, WritesWhatItReads) {
    const ProjectManifest read = ParseProjectManifest(kDocsManifest).Value();
    const std::string written = WriteProjectManifest(read);
    const auto again = ParseProjectManifest(written);
    ASSERT_TRUE(again) << again.GetError().message;
    EXPECT_EQ(again.Value(), read);
    EXPECT_EQ(written.find("  \"runic\": 1,"), 2u); // two spaces of indent, in RunicDocs' order
}

TEST(ProjectManifest, OptionalSectionsTakeTheirDefaults) {
    const auto manifest =
        ParseProjectManifest(R"({"runic": 1, "engine": "0.1.0", "game": {"name": "Small"}})");
    ASSERT_TRUE(manifest) << manifest.GetError().message;
    EXPECT_EQ(manifest.Value().paths, ProjectPaths{});
    EXPECT_EQ(manifest.Value().cook, CookSettings{});
    EXPECT_TRUE(manifest.Value().executables.server.empty());
}

TEST(ProjectManifest, EveryProblemNamesItsMemberAllAtOnce) {
    const auto manifest = ParseProjectManifest(R"({
        "runic": 2,
        "game": {"name": ""},
        "paths": {"content": 5, "art": "Art"},
        "cook": {"cellSizeM": -3},
        "extra": true})");
    ASSERT_FALSE(manifest);
    const std::string &error = manifest.GetError().message;
    EXPECT_NE(error.find("runic: format 2 is not the one this Engine reads (1)"), std::string::npos)
        << error;
    EXPECT_NE(error.find("engine: is missing"), std::string::npos) << error;
    EXPECT_NE(error.find("game.name: must be a non-empty string"), std::string::npos) << error;
    EXPECT_NE(error.find("paths.content: must be a non-empty string"), std::string::npos) << error;
    EXPECT_NE(error.find("paths.art: is not a manifest member"), std::string::npos) << error;
    EXPECT_NE(error.find("cook.cellSizeM: must be more than 0"), std::string::npos) << error;
    EXPECT_NE(error.find("extra: is not a manifest member"), std::string::npos) << error;
    EXPECT_NE(error.find("7 problem(s)"), std::string::npos) << error;
}

TEST(ProjectManifest, BrokenJsonIsReportedWithItsLineAndColumn) {
    for (const std::string_view broken :
         {R"({"runic": 1,})", R"({"runic": 01})", R"({"a": 1, "a": 2})", "{\"a\": \"\x01\"}",
          R"(["x" "y"])", R"({"a": "\ud800"})", R"({"a": tru})", R"({} {})"}) {
        const auto manifest = ParseProjectManifest(broken);
        ASSERT_FALSE(manifest) << broken;
        EXPECT_NE(manifest.GetError().message.find("is not valid JSON: line 1, column"),
                  std::string::npos)
            << manifest.GetError().message;
    }
    const auto multiline = ParseProjectManifest("{\n  \"runic\": 1\n  \"engine\": \"x\"\n}");
    EXPECT_NE(multiline.GetError().message.find("line 3, column 3"), std::string::npos)
        << multiline.GetError().message;
}

TEST(ProjectManifest, StringsKeepTheirEscapesAndUnicode) {
    const auto manifest = ParseProjectManifest(
        R"({"runic": 1, "engine": "0.1.0", "game": {"name": "Rune \"é\" 😀 \\ /"}})");
    ASSERT_TRUE(manifest) << manifest.GetError().message;
    EXPECT_EQ(manifest.Value().gameName, "Rune \"\xC3\xA9\" \xF0\x9F\x98\x80 \\ /");
    EXPECT_EQ(ParseProjectManifest(WriteProjectManifest(manifest.Value())).Value().gameName,
              manifest.Value().gameName);
}

TEST(NewProject, MakesTheTemplatesLayoutWithTheGamesName) {
    ScratchFolder folder("new");
    const RunicProject project = NewProject(folder.Path(), "Moon Harbor");
    EXPECT_EQ(project.manifest.gameName, "Moon Harbor");
    EXPECT_EQ(project.manifest.engine, "0.1.0");

    for (const char *path :
         {"project.godot", "runic.project.json", "game_editor.gdextension", "CMakeLists.txt",
          "Content", "Data", "Tools", "Source/Shared", "Source/Server", "Source/Client",
          "Source/Editor/GameEditor.cpp", "Services", "Build"}) {
        EXPECT_TRUE(fs::exists(folder.Path() / path)) << path;
    }
    // Code, services and builds stay out of Godot's importer.
    for (const char *ignored : {"Source/.gdignore", "Services/.gdignore", "Build/.gdignore"}) {
        EXPECT_TRUE(fs::is_regular_file(folder.Path() / ignored)) << ignored;
    }
    const std::string godot = ReadAll(folder.Path() / "project.godot");
    EXPECT_NE(godot.find("config/name=\"Moon Harbor\""), std::string::npos);
    EXPECT_EQ(godot.find("{{GAME_NAME}}"), std::string::npos);
    EXPECT_NE(ReadAll(folder.Path() / "game_editor.gdextension").find("reloadable = true"),
              std::string::npos);

    const auto opened = OpenProject(folder.Path());
    ASSERT_TRUE(opened) << opened.GetError().message;
    EXPECT_EQ(opened.Value().manifest, project.manifest);
    EXPECT_TRUE(IsRunicProject(folder.Path()));
}

TEST(NewProject, KeepsTheProjectGodotThatTheProjectManagerMade) {
    ScratchFolder folder("keep");
    WriteAll(folder.Path() / "project.godot", "; made by the project manager\n");
    fs::create_directories(folder.Path() / ".godot");
    NewProject(folder.Path());
    EXPECT_EQ(ReadAll(folder.Path() / "project.godot"), "; made by the project manager\n");
    EXPECT_TRUE(IsRunicProject(folder.Path()));
}

TEST(NewProject, RefusesAFolderThatHoldsAnythingElse) {
    ScratchFolder folder("busy");
    WriteAll(folder.Path() / "notes.txt", "mine");
    NewProjectSpec spec{kTemplate, folder.Path(), "Busy", "0.1.0"};
    const auto project = InitializeProject(spec);
    ASSERT_FALSE(project);
    EXPECT_NE(project.GetError().message.find("notes.txt is there"), std::string::npos);
    EXPECT_FALSE(IsRunicProject(folder.Path()));
}

TEST(NewProject, RefusesANameThatIsNotAGameName) {
    ScratchFolder folder("name");
    for (const char *name : {"", " Leading", "Bad/Name", "Quote\"Name"}) {
        const auto project = InitializeProject({kTemplate, folder.Path(), name, "0.1.0"});
        EXPECT_FALSE(project) << name;
    }
    EXPECT_FALSE(IsRunicProject(folder.Path()));
}

TEST(OpenProject, AFolderWithoutAManifestIsNotARunicProject) {
    ScratchFolder folder("plain");
    EXPECT_FALSE(IsRunicProject(folder.Path()));
    const auto project = OpenProject(folder.Path());
    ASSERT_FALSE(project);
    EXPECT_NE(project.GetError().message.find("is not a Runic project"), std::string::npos);
}

TEST(IdeBridge, WritesGoWorkVsCodeTasksAndLaunchConfigurations) {
    ScratchFolder folder("ide");
    RunicProject project = NewProject(folder.Path());
    project.manifest.executables = {"TestGameServer", "TestGameClient"};
    WriteAll(folder.Path() / "Services/trade/go.mod", "module example/trade\n\ngo 1.26.1\n");
    WriteAll(folder.Path() / "Services/mail/go.mod", "module example/mail\n\ngo 1.25\n");
    WriteAll(folder.Path() / "Services/notes/README.md", "not a module");

    const auto report = WriteIdeFiles(project, {});
    ASSERT_TRUE(report) << report.GetError().message;
    EXPECT_EQ(ReadAll(folder.Path() / "go.work"),
              "go 1.26.1\n\nuse (\n\t./Services/mail\n\t./Services/trade\n)\n");
    const std::string tasks = ReadAll(folder.Path() / ".vscode/tasks.json");
    EXPECT_NE(tasks.find("\"Runic: build\""), std::string::npos);
    EXPECT_NE(tasks.find("\"--preset\""), std::string::npos);
    EXPECT_NE(tasks.find("Runic: test Go services"), std::string::npos);
    const std::string launch = ReadAll(folder.Path() / ".vscode/launch.json");
    EXPECT_NE(launch.find("${workspaceFolder}/Build/debug/bin/TestGameServer"), std::string::npos);
    EXPECT_NE(launch.find("Runic: debug TestGameClient"), std::string::npos);
    // No build yet: compile_commands.json is left for later, and said so.
    ASSERT_EQ(report.Value().notes.size(), 1u);
    EXPECT_NE(report.Value().notes[0].find("configure the project first"), std::string::npos);
}

TEST(IdeBridge, PointsClangdAtTheBuildsCompileCommands) {
    ScratchFolder folder("clangd");
    const RunicProject project = NewProject(folder.Path());
    WriteAll(folder.Path() / "Build/debug/compile_commands.json", "[]\n");
    const auto report = WriteIdeFiles(project, {});
    ASSERT_TRUE(report) << report.GetError().message;
    EXPECT_EQ(ReadAll(folder.Path() / "compile_commands.json"), "[]\n");
    EXPECT_TRUE(fs::is_symlink(folder.Path() / "compile_commands.json"));
    ASSERT_TRUE(WriteIdeFiles(project, {})); // again: the link is the bridge's, so it is replaced
}

TEST(IdeBridge, NeverReplacesFilesItDidNotWrite) {
    ScratchFolder folder("theirs");
    const RunicProject project = NewProject(folder.Path());
    WriteAll(folder.Path() / ".vscode/tasks.json", "{\"version\": \"2.0.0\", \"tasks\": []}\n");
    WriteAll(folder.Path() / "Build/debug/compile_commands.json", "[]\n");
    WriteAll(folder.Path() / "compile_commands.json", "[] # mine\n");
    const auto report = WriteIdeFiles(project, {});
    ASSERT_TRUE(report) << report.GetError().message;
    EXPECT_EQ(ReadAll(folder.Path() / ".vscode/tasks.json"),
              "{\"version\": \"2.0.0\", \"tasks\": []}\n");
    EXPECT_EQ(ReadAll(folder.Path() / "compile_commands.json"), "[] # mine\n");
    EXPECT_EQ(report.Value().notes.size(), 2u);
    EXPECT_TRUE(fs::exists(folder.Path() / ".vscode/launch.json")); // that one was free
}
