// RunicEditor: the Godot editor run through LibGodot, with the Runic tools compiled in (GodotTools).
//
//   RunicEditor                                     Godot's project manager (New and Open)
//   RunicEditor --editor --path <project>           opens a project, as the project manager does
//   RunicEditor --headless --editor --path <project> --quit-after 120
//                                                   the GDT-1 spike check in CI: open, load the plugin, exit
//
// Every argument goes to Godot unchanged: the project manager opens a project by relaunching this
// executable with --path, so main() never consumes or rewrites arguments.

#include "GodotRuntime.hpp"
#include "GodotToolsModule.hpp"

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

namespace {

bool HasArgument(const std::vector<std::string>& arguments, const char* name) {
    return std::find(arguments.begin(), arguments.end(), name) != arguments.end();
}

// Godot's Play buttons relaunch the editor executable without --editor and with --editor-pid to run the
// project as a game. In a Runic project the game runs through Runic's Run (GDT-9, slice 13), never inside
// Godot; until then this safety net stops the relaunch.
bool IsGodotPlayRequest(const std::vector<std::string>& arguments) {
    return HasArgument(arguments, "--editor-pid") && !HasArgument(arguments, "--editor") &&
           !HasArgument(arguments, "-e");
}

} // namespace

int main(int argc, char** argv) {
    using Runic::GodotBridge::GodotRuntime;
    using Runic::GodotBridge::GodotTarget;

    const std::vector<std::string> arguments = GodotRuntime::CommandLine(argc, argv);
    if (IsGodotPlayRequest(arguments)) {
        std::fprintf(stderr, "RUNIC: Godot's Play runs the project inside Godot; a Runic project runs through "
                             "Runic's Run instead (not available before slice 13).\n");
        return 0;
    }

    GodotRuntime godot;
    Runic::GodotTools::RegisterEditorTools(godot);

    const std::string library = GodotRuntime::DefaultLibraryPath(GodotTarget::Editor);
    if (!godot.Load(library) || !godot.Create(arguments) || !godot.Start()) {
        std::fprintf(stderr, "RUNIC: RunicEditor could not start Godot: %s\n", godot.LastError().c_str());
        godot.Destroy();
        return 1;
    }

    // The editor paces its own frames; one iteration is one Godot frame. True means the editor quit.
    while (!godot.Iterate()) {
    }

    godot.Destroy();
    std::printf("RUNIC: RunicEditor exited cleanly after %llu frames\n",
                static_cast<unsigned long long>(godot.FrameCount()));
    return 0;
}
