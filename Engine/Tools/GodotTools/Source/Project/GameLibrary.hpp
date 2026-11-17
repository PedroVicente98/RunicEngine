#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

// The project's editor library (GDT-2; RunicDocs › Godot integration › The game's editor library):
// the game's marker nodes and docks, a GDExtension the project declares in game_editor.gdextension
// (reloadable = true). Godot loads it when the editor opens the project; this watcher loads it if
// Godot has not, and reloads it when a build replaces the library file, so the editor never
// restarts for a rebuilt editor library. Godot's main thread only. Private to Runic::GodotTools.

namespace Runic::GodotTools::Detail {

class GameLibrary {
  public:
    // Reads res://game_editor.gdextension and finds the library it names for this platform.
    void Open();
    bool HasExtension() const noexcept { return m_HasExtension; }
    bool IsLoaded() const;
    const std::filesystem::path &LibraryFile() const noexcept { return m_Library; }

    // Called a few times a second: loads the library when Godot has not, and reloads it when the
    // file changed and stayed the same for one more look (a build still writing it is not loaded).
    void Poll();
    // Reloads now. False when there is nothing to reload or Godot refused.
    bool Reload();
    std::uint64_t Reloads() const noexcept { return m_Reloads; }

  private:
    bool m_HasExtension = false;
    std::filesystem::path m_Library;
    std::filesystem::file_time_type m_Loaded{};  // the file's time when it was last (re)loaded
    std::filesystem::file_time_type m_Changed{}; // a newer time seen once, waiting to settle
    bool m_HasChange = false;
    std::uint64_t m_Reloads = 0;
};

} // namespace Runic::GodotTools::Detail
