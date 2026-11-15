#pragma once

#include <string>
#include <string_view>
#include <vector>

// The Godot command-line flags Runic programs read or add (Godot 4.7.2's --help); every other
// argument reaches Godot unchanged. The frame limit flag belongs to the Engine
// (Application::kQuitAfterFlag), and Godot reads it too.

namespace Runic::GodotBridge {

inline constexpr std::string_view kPathFlag = "--path";         // the project folder to run
inline constexpr std::string_view kHeadlessFlag = "--headless"; // no window and no rendering
inline constexpr std::string_view kEditorFlag = "--editor";     // open the project in the editor
inline constexpr std::string_view kEditorShortFlag = "-e";      // kEditorFlag, short
// A run the editor's Play button started, with the editor's process ID.
inline constexpr std::string_view kEditorPidFlag = "--editor-pid";

// Puts kPathFlag and folder right after argv[0], where Godot reads its project folder.
void InsertProjectPath(std::vector<std::string> &arguments, std::string_view folder);

} // namespace Runic::GodotBridge
