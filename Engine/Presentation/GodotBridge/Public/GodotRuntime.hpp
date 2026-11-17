#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace Runic::GodotBridge {

// The two libgodot targets a host can load (RunicDocs › Godot integration › Executables and Godot targets).
enum class GodotTarget {
    Editor,    // editor, importers and project manager: the RunicEditor and the development GameClient
    Template,  // runtime only (rendering, audio, input, resource loading): the shipping GameClient
};

// Godot's GDExtension initialization levels, in the order Godot initializes them.
enum class GodotInitLevel { Core, Servers, Scene, Editor };

// The LibGodot instance of a process. The host's main() owns the loop and Godot is a library stepped once
// per frame: Load the library, Create the instance with the command line, Start it, call Iterate() until it
// returns true, then Destroy it. Every call happens on the thread that called Create().
//
// LibGodot allows one instance per process, and never a second one after the first is destroyed.
class GodotRuntime {
public:
    using LevelHook = std::function<void(GodotInitLevel)>;

    GodotRuntime();
    ~GodotRuntime();  // Destroy() if the instance still exists
    GodotRuntime(const GodotRuntime&) = delete;
    GodotRuntime& operator=(const GodotRuntime&) = delete;

    // Hooks that run inside Godot's initialization, once per level from Scene upwards (Editor only in the
    // editor target), and in reverse order on shutdown. Presentation and tool classes register here.
    // Add them before Create().
    void AddInitializeHook(LevelHook hook);
    void AddDeinitializeHook(LevelHook hook);

    // Opens the library and finds its two entry points (libgodot_create_godot_instance and
    // libgodot_destroy_godot_instance).
    bool Load(const std::string& libraryPath);

    // Main::setup with these arguments. arguments[0] must be the executable path; the others are ordinary
    // Godot arguments (--path, --editor, --headless, --quit-after...) and reach Godot unchanged, because the
    // project manager and the editor relaunch the host executable with their own arguments.
    bool Create(const std::vector<std::string>& arguments);

    // Window (unless --headless), renderer and audio, then the main scene, the editor or the project manager.
    bool Start();

    // One Godot frame: events, input, UI, audio, render, present. Returns true when Godot asks to quit.
    bool Iterate();

    // Stops the instance and runs Main::cleanup. Safe to call twice.
    void Destroy();

    bool IsLoaded() const;
    bool IsCreated() const;
    bool IsStarted() const;
    std::uint64_t FrameCount() const;
    const std::string& LastError() const;

    // libgodot.<platform>.<target>.<arch>.<ext>, the name the build stages next to the executables.
    static std::string LibraryFileName(GodotTarget target);
    // The library next to the running executable (for Template: template_release, else template_debug).
    static std::string DefaultLibraryPath(GodotTarget target);
    static std::string ExecutableDirectory();
    // The command line as UTF-8 (on Windows, main()'s argv is in the ANSI code page).
    static std::vector<std::string> CommandLine(int argc, char** argv);

private:
    struct Impl;
    std::unique_ptr<Impl> m_Impl;
};

} // namespace Runic::GodotBridge
