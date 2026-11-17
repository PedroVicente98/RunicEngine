// The project's editor library (GameLibrary.hpp), through Godot's GDExtensionManager.

#include "Project/GameLibrary.hpp"

#include <system_error>

#include <godot_cpp/classes/config_file.hpp>
#include <godot_cpp/classes/gd_extension_manager.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>

#include "Log.hpp"
#include "RunicProject.hpp"

namespace Runic::GodotTools::Detail {

namespace {

constexpr const char *kProjectRoot = "res://";
constexpr const char *kLibrariesSection = "libraries";
constexpr char kFeatureSeparator = '.';

godot::String ExtensionPath() {
    return godot::String(kProjectRoot) +
           godot::String(std::string(kGameEditorExtensionFileName).c_str());
}

std::string ToStd(const godot::String &text) { return text.utf8().get_data(); }

// The [libraries] entry whose feature tags this editor has, such as linux.editor.x86_64.
godot::String LibraryFor(const godot::Ref<godot::ConfigFile> &config) {
    if (!config->has_section(kLibrariesSection)) {
        return {};
    }
    const godot::PackedStringArray keys = config->get_section_keys(kLibrariesSection);
    godot::OS *os = godot::OS::get_singleton();
    for (int64_t i = 0; i < keys.size(); ++i) {
        const godot::PackedStringArray tags = keys[i].split(godot::String::chr(kFeatureSeparator));
        bool matches = true;
        for (int64_t t = 0; t < tags.size() && matches; ++t) {
            matches = os->has_feature(tags[t]);
        }
        if (matches) {
            return config->get_value(kLibrariesSection, keys[i]);
        }
    }
    return {};
}

std::filesystem::file_time_type TimeOf(const std::filesystem::path &file) {
    std::error_code error;
    const auto time = std::filesystem::last_write_time(file, error);
    return error ? std::filesystem::file_time_type{} : time;
}

const char *StatusText(godot::GDExtensionManager::LoadStatus status) {
    switch (status) {
    case godot::GDExtensionManager::LOAD_STATUS_OK:
        return "ok";
    case godot::GDExtensionManager::LOAD_STATUS_FAILED:
        return "failed";
    case godot::GDExtensionManager::LOAD_STATUS_ALREADY_LOADED:
        return "already loaded";
    case godot::GDExtensionManager::LOAD_STATUS_NOT_LOADED:
        return "not loaded";
    case godot::GDExtensionManager::LOAD_STATUS_NEEDS_RESTART:
        return "needs a restart";
    }
    return "unknown";
}

} // namespace

void GameLibrary::Open() {
    godot::Ref<godot::ConfigFile> config;
    config.instantiate();
    m_HasExtension = config->load(ExtensionPath()) == godot::OK;
    if (!m_HasExtension) {
        return;
    }
    const godot::String library = LibraryFor(config);
    if (library.is_empty()) {
        RUNIC_LOG_WARNING("RUNIC: {} names no library for this editor's platform",
                          kGameEditorExtensionFileName);
        return;
    }
    m_Library = ToStd(godot::ProjectSettings::get_singleton()->globalize_path(library));
    m_Loaded = TimeOf(m_Library);
    RUNIC_LOG_INFO("RUNIC: game editor library {} ({})", ToStd(library),
                   IsLoaded() ? "loaded" : "not loaded yet");
}

bool GameLibrary::IsLoaded() const {
    return m_HasExtension &&
           godot::GDExtensionManager::get_singleton()->is_extension_loaded(ExtensionPath());
}

void GameLibrary::Poll() {
    if (!m_HasExtension || m_Library.empty()) {
        return;
    }
    const auto now = TimeOf(m_Library);
    if (now == std::filesystem::file_time_type{}) {
        return; // not built yet, or being replaced
    }
    if (!IsLoaded()) {
        const auto status =
            godot::GDExtensionManager::get_singleton()->load_extension(ExtensionPath());
        m_Loaded = now;
        RUNIC_LOG_INFO("RUNIC: game editor library loaded ({})", StatusText(status));
        return;
    }
    if (now == m_Loaded) {
        m_HasChange = false;
        return;
    }
    if (!m_HasChange || now != m_Changed) {
        m_Changed = now; // seen once: wait one more look for the build to finish writing it
        m_HasChange = true;
        return;
    }
    Reload();
}

bool GameLibrary::Reload() {
    if (!IsLoaded()) {
        return false;
    }
    const auto status =
        godot::GDExtensionManager::get_singleton()->reload_extension(ExtensionPath());
    m_Loaded = TimeOf(m_Library);
    m_HasChange = false;
    if (status != godot::GDExtensionManager::LOAD_STATUS_OK) {
        RUNIC_LOG_WARNING("RUNIC: game editor library could not be reloaded ({})",
                          StatusText(status));
        return false;
    }
    ++m_Reloads;
    RUNIC_LOG_INFO("RUNIC: game editor library reloaded ({} so far)", m_Reloads);
    return true;
}

} // namespace Runic::GodotTools::Detail
