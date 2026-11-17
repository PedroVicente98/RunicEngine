#pragma once

#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>

#include "Result.hpp"

// Process configuration: the typed settings a process reads once at start. Each setting has a
// default and may be set by the configuration file, then the environment, then the command line;
// a later source wins over an earlier one. Everything wrong is reported with where it came from
// (a line of the file, an environment variable, a command-line argument), all at once, so a typo
// never becomes a silent default (RunicDocs › Modules, work packages › CORE-8).
//
//   file          key = value lines; '#' starts a comment line; blank lines are skipped
//   environment   RUNIC_CFG_<KEY>, the key in upper case with '_' for '.':
//   RUNIC_CFG_SIMULATION_WORKERS command line  --set key=value, as often as needed; --config <path>
//   names the file

namespace Runic::Core {

// Where a setting's value came from, in the order of precedence: each wins over the ones before.
enum class ConfigSource : std::uint8_t { Default, File, Environment, CommandLine };

// "default", "file", "environment" or "command line".
std::string_view ConfigSourceName(ConfigSource source) noexcept;

enum class SettingKind : std::uint8_t {
    Bool, // true or false
    Int,  // a whole number in decimal, inside the range the setting declares
    Text, // any text
};

// The flag that names the configuration file and the flag that sets one value.
inline constexpr std::string_view kConfigFileFlag = "--config";
inline constexpr std::string_view kConfigSetFlag = "--set";

// The start of every environment variable that sets a value. A variable with this prefix that
// names no setting is reported; every other variable is not the configuration's.
inline constexpr std::string_view kConfigEnvironmentPrefix = "RUNIC_CFG_";

// How a Bool is written in every source.
inline constexpr std::string_view kConfigTrue = "true";
inline constexpr std::string_view kConfigFalse = "false";

// The handle of a declared setting; reading through it needs no lookup by name.
template <SettingKind Kind> class Setting {
  public:
    constexpr Setting() noexcept = default;
    constexpr bool IsValid() const noexcept { return m_Index != kInvalidIndex; }
    constexpr std::uint32_t Index() const noexcept { return m_Index; }

  private:
    friend class ProcessConfig;

    static constexpr std::uint32_t kInvalidIndex = 0xFFFF'FFFF;

    constexpr explicit Setting(std::uint32_t index) noexcept : m_Index(index) {}

    std::uint32_t m_Index = kInvalidIndex;
};

using BoolSetting = Setting<SettingKind::Bool>;
using IntSetting = Setting<SettingKind::Int>;
using TextSetting = Setting<SettingKind::Text>;

// The values an Int setting accepts, both ends included.
struct IntRange {
    std::int64_t min = 0;
    std::int64_t max = 0;
};

// One thing wrong in the sources: where it is and what is wrong with it.
struct ConfigProblem {
    std::string source;  // "server.cfg:3", "environment RUNIC_CFG_X", "command line --set a=b"
    std::string message; // "unknown setting 'a'", "simulation.workers: 0 is outside 1 to 64"
};

// What Load reads. Each part may be empty.
struct ConfigInputs {
    std::string_view fileText;                // the configuration file's content
    std::string_view fileName;                // how problems name the file
    std::span<const std::string> environment; // "NAME=value" entries
    std::span<const std::string> arguments;   // the command line; the program's name may be first
};

// A setting as a tool or a start-up log lists it.
struct SettingInfo {
    std::string_view key;
    std::string_view help;
    SettingKind kind = SettingKind::Bool;
    ConfigSource source = ConfigSource::Default;
    std::string value;        // as it would be written in a source
    std::string defaultValue; // the same for the default
};

class ProcessConfig {
  public:
    ProcessConfig();
    ~ProcessConfig();

    ProcessConfig(const ProcessConfig &) = delete;
    ProcessConfig &operator=(const ProcessConfig &) = delete;

    // Declares a setting, before Load. A key is lowercase words joined by dots, with digits and
    // underscores ("simulation.workers"); declaring one twice is a programming error.
    BoolSetting DeclareBool(std::string_view key, bool defaultValue, std::string_view help);
    IntSetting DeclareInt(std::string_view key, std::int64_t defaultValue, IntRange range,
                          std::string_view help);
    TextSetting DeclareText(std::string_view key, std::string_view defaultValue,
                            std::string_view help);

    // Resolves every setting from the inputs, once. On failure the error lists every problem, one
    // per line, and Problems() has them one by one; settings no problem touched are resolved.
    Result<void> Load(const ConfigInputs &inputs);

    // The same for the running process: reads the file --config names (no flag, no file) and the
    // process's environment, then loads with the given command line.
    Result<void> LoadForProcess(std::span<const std::string> arguments);

    bool IsLoaded() const noexcept;
    std::span<const ConfigProblem> Problems() const noexcept;

    // The resolved values; the defaults before Load. Thread-safe once loaded. The text stays
    // valid while the configuration lives.
    bool Get(BoolSetting setting) const noexcept;
    std::int64_t Get(IntSetting setting) const noexcept;
    std::string_view Get(TextSetting setting) const noexcept;

    template <SettingKind Kind> ConfigSource SourceOf(Setting<Kind> setting) const noexcept {
        return SourceAt(setting.Index());
    }

    // Every setting in the order it was declared, for a start-up log or a listing.
    std::uint32_t Count() const noexcept;
    SettingInfo Describe(std::uint32_t index) const;

    // The environment variable that sets a key: "simulation.workers" gives
    // "RUNIC_CFG_SIMULATION_WORKERS".
    static std::string EnvironmentName(std::string_view key);

  private:
    struct Entry;
    struct State;

    std::uint32_t Declare(SettingKind kind, std::string_view key, std::string_view help);
    ConfigSource SourceAt(std::uint32_t index) const noexcept;

    std::unique_ptr<State> m_State;
};

} // namespace Runic::Core
