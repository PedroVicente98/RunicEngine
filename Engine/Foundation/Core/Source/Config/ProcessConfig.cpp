// Process configuration (ProcessConfig.hpp): the declared settings, the three parsers (file,
// environment, command line) and the problems they report.

#include "ProcessConfig.hpp"

#include <algorithm>
#include <charconv>
#include <format>
#include <fstream>
#include <iterator>
#include <system_error>
#include <vector>

#if defined(_WIN32)
#include <stdlib.h> // _environ
#else
extern char **environ; // POSIX: the process's "NAME=value" entries
#endif

#include "Assert.hpp"

namespace Runic::Core {

namespace {

constexpr std::string_view kDefaultName = "default";
constexpr std::string_view kFileName = "file";
constexpr std::string_view kEnvironmentName = "environment";
constexpr std::string_view kCommandLineName = "command line";

constexpr char kAssign = '=';      // between a key and its value, in every source
constexpr char kCommentMark = '#'; // starts a comment line in the file
constexpr char kLineEnd = '\n';
constexpr char kKeySeparator = '.';         // between the words of a key
constexpr char kEnvironmentSeparator = '_'; // what the separator becomes in a variable's name
constexpr std::string_view kBlanks = " \t\r";
constexpr std::string_view kUnnamedFile = "configuration file";
constexpr std::uint32_t kFirstLine = 1;
constexpr std::uint32_t kNotInFile = 0; // the line of a setting the file has not set
constexpr int kDecimal = 10;

std::string_view Trim(std::string_view text) noexcept {
    const std::size_t first = text.find_first_not_of(kBlanks);
    if (first == std::string_view::npos) {
        return {};
    }
    return text.substr(first, text.find_last_not_of(kBlanks) - first + 1);
}

bool IsLower(char character) noexcept { return character >= 'a' && character <= 'z'; }
bool IsDigit(char character) noexcept { return character >= '0' && character <= '9'; }

// A key is lowercase words joined by dots; a word starts with a letter and may hold digits and
// underscores.
bool IsKey(std::string_view key) noexcept {
    bool wordStart = true;
    for (const char character : key) {
        if (wordStart) {
            if (!IsLower(character)) {
                return false;
            }
            wordStart = false;
        } else if (character == kKeySeparator) {
            wordStart = true;
        } else if (!IsLower(character) && !IsDigit(character) &&
                   character != kEnvironmentSeparator) {
            return false;
        }
    }
    return !key.empty() && !wordStart;
}

char ToUpper(char character) noexcept {
    return IsLower(character) ? static_cast<char>(character - 'a' + 'A') : character;
}

} // namespace

std::string_view ConfigSourceName(ConfigSource source) noexcept {
    switch (source) {
    case ConfigSource::Default:
        return kDefaultName;
    case ConfigSource::File:
        return kFileName;
    case ConfigSource::Environment:
        return kEnvironmentName;
    case ConfigSource::CommandLine:
        return kCommandLineName;
    }
    return kDefaultName;
}

struct ProcessConfig::Entry {
    SettingKind kind = SettingKind::Bool;
    std::string key;
    std::string help;
    std::string environmentName;
    ConfigSource source = ConfigSource::Default;
    std::uint32_t fileLine = kNotInFile;

    bool boolValue = false;
    bool boolDefault = false;
    std::int64_t intValue = 0;
    std::int64_t intDefault = 0;
    IntRange range;
    std::string textValue;
    std::string textDefault;
};

struct ProcessConfig::State {
    std::vector<Entry> entries;
    std::vector<ConfigProblem> problems;
    bool loaded = false;

    Entry *Find(std::string_view key) noexcept {
        const auto found = std::ranges::find(entries, key, &Entry::key);
        return found != entries.end() ? &*found : nullptr;
    }

    Entry *FindByEnvironmentName(std::string_view name) noexcept {
        const auto found = std::ranges::find(entries, name, &Entry::environmentName);
        return found != entries.end() ? &*found : nullptr;
    }

    void Report(std::string source, std::string message) {
        problems.push_back(ConfigProblem{std::move(source), std::move(message)});
    }

    // Gives a setting the value a source wrote, or reports why the text is not one.
    void Assign(Entry &entry, std::string_view text, ConfigSource source,
                const std::string &sourceText) {
        switch (entry.kind) {
        case SettingKind::Bool:
            if (text != kConfigTrue && text != kConfigFalse) {
                Report(sourceText, std::format("{}: '{}' is not {} or {}", entry.key, text,
                                               kConfigTrue, kConfigFalse));
                return;
            }
            entry.boolValue = text == kConfigTrue;
            break;
        case SettingKind::Int: {
            std::int64_t value = 0;
            const char *end = text.data() + text.size();
            const auto parsed = std::from_chars(text.data(), end, value, kDecimal);
            if (text.empty() || parsed.ptr != end || parsed.ec == std::errc::invalid_argument) {
                Report(sourceText, std::format("{}: '{}' is not a whole number", entry.key, text));
                return;
            }
            if (parsed.ec == std::errc::result_out_of_range || value < entry.range.min ||
                value > entry.range.max) {
                Report(sourceText, std::format("{}: {} is outside {} to {}", entry.key, text,
                                               entry.range.min, entry.range.max));
                return;
            }
            entry.intValue = value;
            break;
        }
        case SettingKind::Text:
            entry.textValue = text;
            break;
        }
        entry.source = source;
    }

    void LoadFile(std::string_view text, std::string_view fileName) {
        const std::string_view name = fileName.empty() ? kUnnamedFile : fileName;
        std::uint32_t lineNumber = kFirstLine;
        for (std::size_t start = 0; start < text.size(); ++lineNumber) {
            const std::size_t end = std::min(text.find(kLineEnd, start), text.size());
            const std::string_view line = Trim(text.substr(start, end - start));
            start = end + 1;
            if (line.empty() || line.front() == kCommentMark) {
                continue;
            }
            const std::string where = std::format("{}:{}", name, lineNumber);
            const std::size_t assign = line.find(kAssign);
            const std::string_view key = Trim(line.substr(0, assign));
            if (assign == std::string_view::npos || key.empty()) {
                Report(where, std::format("'{}' is not a 'key {} value' line", line, kAssign));
                continue;
            }
            Entry *entry = Find(key);
            if (entry == nullptr) {
                Report(where, std::format("unknown setting '{}'", key));
                continue;
            }
            if (entry->fileLine != kNotInFile) {
                Report(where, std::format("{} is already set on line {}", key, entry->fileLine));
                continue;
            }
            entry->fileLine = lineNumber;
            Assign(*entry, Trim(line.substr(assign + 1)), ConfigSource::File, where);
        }
    }

    void LoadEnvironment(std::span<const std::string> environment) {
        for (const std::string &variable : environment) {
            const std::string_view text = variable;
            if (!text.starts_with(kConfigEnvironmentPrefix)) {
                continue;
            }
            const std::size_t assign = text.find(kAssign);
            const std::string_view name = text.substr(0, assign);
            const std::string where = std::format("{} {}", kEnvironmentName, name);
            if (assign == std::string_view::npos) {
                Report(where, "has no value");
                continue;
            }
            Entry *entry = FindByEnvironmentName(name);
            if (entry == nullptr) {
                Report(where, "names no setting");
                continue;
            }
            Assign(*entry, text.substr(assign + 1), ConfigSource::Environment, where);
        }
    }

    void LoadCommandLine(std::span<const std::string> arguments) {
        for (std::size_t index = 0; index < arguments.size(); ++index) {
            const std::string_view argument = arguments[index];
            if (argument == kConfigFileFlag) {
                if (index + 1 == arguments.size()) {
                    Report(std::format("{} {}", kCommandLineName, kConfigFileFlag),
                           "needs the path of a configuration file");
                }
                ++index; // the path was read by whoever gave the file's text
                continue;
            }
            if (argument != kConfigSetFlag) {
                continue; // another part of the program reads it
            }
            if (index + 1 == arguments.size()) {
                Report(std::format("{} {}", kCommandLineName, kConfigSetFlag),
                       std::format("needs 'key{}value'", kAssign));
                continue;
            }
            const std::string_view pair = arguments[++index];
            const std::string where =
                std::format("{} {} {}", kCommandLineName, kConfigSetFlag, pair);
            const std::size_t assign = pair.find(kAssign);
            const std::string_view key = pair.substr(0, assign);
            if (assign == std::string_view::npos || key.empty()) {
                Report(where, std::format("is not 'key{}value'", kAssign));
                continue;
            }
            Entry *entry = Find(key);
            if (entry == nullptr) {
                Report(where, std::format("unknown setting '{}'", key));
                continue;
            }
            Assign(*entry, pair.substr(assign + 1), ConfigSource::CommandLine, where);
        }
    }
};

ProcessConfig::ProcessConfig() : m_State(std::make_unique<State>()) {}

ProcessConfig::~ProcessConfig() = default;

std::string ProcessConfig::EnvironmentName(std::string_view key) {
    std::string name(kConfigEnvironmentPrefix);
    for (const char character : key) {
        name.push_back(character == kKeySeparator ? kEnvironmentSeparator : ToUpper(character));
    }
    return name;
}

std::uint32_t ProcessConfig::Declare(SettingKind kind, std::string_view key,
                                     std::string_view help) {
    RUNIC_ASSERT_MSG(!m_State->loaded, "a setting was declared after the configuration was loaded");
    RUNIC_ASSERT_MSG(IsKey(key), "a setting's key is lowercase words joined by dots");
    RUNIC_ASSERT_MSG(m_State->Find(key) == nullptr, "a setting was declared twice");
    Entry entry;
    entry.kind = kind;
    entry.key = key;
    entry.help = help;
    entry.environmentName = EnvironmentName(key);
    RUNIC_ASSERT_MSG(m_State->FindByEnvironmentName(entry.environmentName) == nullptr,
                     "two settings share one environment variable");
    m_State->entries.push_back(std::move(entry));
    return static_cast<std::uint32_t>(m_State->entries.size() - 1);
}

BoolSetting ProcessConfig::DeclareBool(std::string_view key, bool defaultValue,
                                       std::string_view help) {
    const std::uint32_t index = Declare(SettingKind::Bool, key, help);
    m_State->entries[index].boolValue = m_State->entries[index].boolDefault = defaultValue;
    return BoolSetting(index);
}

IntSetting ProcessConfig::DeclareInt(std::string_view key, std::int64_t defaultValue,
                                     IntRange range, std::string_view help) {
    RUNIC_ASSERT_MSG(range.min <= defaultValue && defaultValue <= range.max,
                     "a setting's default is outside its range");
    const std::uint32_t index = Declare(SettingKind::Int, key, help);
    m_State->entries[index].intValue = m_State->entries[index].intDefault = defaultValue;
    m_State->entries[index].range = range;
    return IntSetting(index);
}

TextSetting ProcessConfig::DeclareText(std::string_view key, std::string_view defaultValue,
                                       std::string_view help) {
    const std::uint32_t index = Declare(SettingKind::Text, key, help);
    m_State->entries[index].textValue = m_State->entries[index].textDefault = defaultValue;
    return TextSetting(index);
}

Result<void> ProcessConfig::Load(const ConfigInputs &inputs) {
    RUNIC_ASSERT_MSG(!m_State->loaded, "a configuration is loaded once");
    m_State->loaded = true;
    // In the order of precedence: each source overwrites what the ones before it set.
    m_State->LoadFile(inputs.fileText, inputs.fileName);
    m_State->LoadEnvironment(inputs.environment);
    m_State->LoadCommandLine(inputs.arguments);
    if (m_State->problems.empty()) {
        return {};
    }
    std::string message = std::format("{} configuration problem(s)", m_State->problems.size());
    for (const ConfigProblem &problem : m_State->problems) {
        message += std::format("{}  {}: {}", kLineEnd, problem.source, problem.message);
    }
    return Fail(std::move(message));
}

Result<void> ProcessConfig::LoadForProcess(std::span<const std::string> arguments) {
    std::string fileText;
    std::string fileName;
    bool fileUnreadable = false;
    const auto flag = std::ranges::find(arguments, kConfigFileFlag);
    if (flag != arguments.end() && std::next(flag) != arguments.end()) {
        fileName = *std::next(flag);
        std::ifstream file(fileName, std::ios::binary);
        if (file) {
            fileText.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
        }
        fileUnreadable = !file || file.bad();
    }

    std::vector<std::string> environment;
#if defined(_WIN32)
    char **entries = _environ;
#else
    char **entries = environ;
#endif
    for (; entries != nullptr && *entries != nullptr; ++entries) {
        const std::string_view entry = *entries;
        if (entry.starts_with(kConfigEnvironmentPrefix)) {
            environment.emplace_back(entry);
        }
    }

    if (fileUnreadable) {
        m_State->Report(std::format("{} {} {}", kCommandLineName, kConfigFileFlag, fileName),
                        "the file cannot be read");
    }
    ConfigInputs inputs;
    inputs.fileText = fileText;
    inputs.fileName = fileName;
    inputs.environment = environment;
    inputs.arguments = arguments;
    return Load(inputs);
}

bool ProcessConfig::IsLoaded() const noexcept { return m_State->loaded; }

std::span<const ConfigProblem> ProcessConfig::Problems() const noexcept {
    return m_State->problems;
}

bool ProcessConfig::Get(BoolSetting setting) const noexcept {
    RUNIC_ASSERT(setting.Index() < m_State->entries.size());
    return m_State->entries[setting.Index()].boolValue;
}

std::int64_t ProcessConfig::Get(IntSetting setting) const noexcept {
    RUNIC_ASSERT(setting.Index() < m_State->entries.size());
    return m_State->entries[setting.Index()].intValue;
}

std::string_view ProcessConfig::Get(TextSetting setting) const noexcept {
    RUNIC_ASSERT(setting.Index() < m_State->entries.size());
    return m_State->entries[setting.Index()].textValue;
}

ConfigSource ProcessConfig::SourceAt(std::uint32_t index) const noexcept {
    RUNIC_ASSERT(index < m_State->entries.size());
    return m_State->entries[index].source;
}

std::uint32_t ProcessConfig::Count() const noexcept {
    return static_cast<std::uint32_t>(m_State->entries.size());
}

SettingInfo ProcessConfig::Describe(std::uint32_t index) const {
    RUNIC_ASSERT(index < m_State->entries.size());
    const Entry &entry = m_State->entries[index];
    SettingInfo info;
    info.key = entry.key;
    info.help = entry.help;
    info.kind = entry.kind;
    info.source = entry.source;
    switch (entry.kind) {
    case SettingKind::Bool:
        info.value = entry.boolValue ? kConfigTrue : kConfigFalse;
        info.defaultValue = entry.boolDefault ? kConfigTrue : kConfigFalse;
        break;
    case SettingKind::Int:
        info.value = std::to_string(entry.intValue);
        info.defaultValue = std::to_string(entry.intDefault);
        break;
    case SettingKind::Text:
        info.value = entry.textValue;
        info.defaultValue = entry.textDefault;
        break;
    }
    return info;
}

} // namespace Runic::Core
