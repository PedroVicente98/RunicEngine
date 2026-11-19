// CommandLine (CommandLine.hpp).

#include "CommandLine.hpp"

#include <charconv>
#include <utility>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <shellapi.h> // after windows.h, which it needs
#endif

namespace Runic::Application {

namespace {

// Arguments[0] is the executable's path; flags come after it.
constexpr std::size_t kFirstArgument = 1;
// What joins a flag to its value in one argument: --flag=value.
constexpr std::string_view kValueSeparator = "=";

#if defined(_WIN32)
// WideCharToMultiByte's arguments: no flags, an input that ends at its null character, and an
// output size that only asks for the size needed. That size counts the terminating null.
constexpr DWORD kNoConversionFlags = 0;
constexpr int kNullTerminated = -1;
constexpr int kMeasureOnly = 0;
constexpr int kTerminatorBytes = 1;
#endif

} // namespace

CommandLine::CommandLine(std::vector<std::string> arguments) : m_Arguments(std::move(arguments)) {}

CommandLine CommandLine::FromMain(int argc, char **argv) {
    std::vector<std::string> arguments;
#if defined(_WIN32)
    (void)argc;
    (void)argv;
    int count = 0;
    LPWSTR *wide = CommandLineToArgvW(GetCommandLineW(), &count);
    for (int i = 0; i < count; ++i) {
        const int bytes = WideCharToMultiByte(CP_UTF8, kNoConversionFlags, wide[i], kNullTerminated,
                                              nullptr, kMeasureOnly, nullptr, nullptr);
        std::string utf8;
        if (bytes > kTerminatorBytes) {
            // The null goes where std::string keeps its own.
            utf8.resize(static_cast<std::size_t>(bytes - kTerminatorBytes));
            WideCharToMultiByte(CP_UTF8, kNoConversionFlags, wide[i], kNullTerminated, utf8.data(),
                                bytes, nullptr, nullptr);
        }
        arguments.push_back(std::move(utf8));
    }
    LocalFree(wide);
#else
    for (int i = 0; i < argc; ++i) {
        arguments.emplace_back(argv[i]);
    }
#endif
    return CommandLine(std::move(arguments));
}

const std::vector<std::string> &CommandLine::Arguments() const noexcept { return m_Arguments; }

bool CommandLine::Has(std::string_view flag) const noexcept {
    for (std::size_t i = kFirstArgument; i < m_Arguments.size(); ++i) {
        if (m_Arguments[i] == flag) {
            return true;
        }
    }
    return false;
}

std::optional<std::string_view> CommandLine::ValueOf(std::string_view flag) const noexcept {
    for (std::size_t i = kFirstArgument; i < m_Arguments.size(); ++i) {
        const std::string_view argument = m_Arguments[i];
        if (argument == flag) {
            const std::size_t next = i + 1;
            if (next < m_Arguments.size()) {
                return std::string_view(m_Arguments[next]);
            }
            return std::nullopt;
        }
        if (argument.starts_with(flag) &&
            argument.substr(flag.size()).starts_with(kValueSeparator)) {
            return argument.substr(flag.size() + kValueSeparator.size());
        }
    }
    return std::nullopt;
}

std::optional<std::uint64_t> CommandLine::QuitAfterFrames() const noexcept {
    const std::optional<std::string_view> value = ValueOf(kQuitAfterFlag);
    if (!value || value->empty()) {
        return std::nullopt;
    }
    std::uint64_t frames = 0;
    const auto [end, error] = std::from_chars(value->data(), value->data() + value->size(), frames);
    if (error != std::errc() || end != value->data() + value->size()) {
        return std::nullopt;
    }
    return frames;
}

} // namespace Runic::Application
