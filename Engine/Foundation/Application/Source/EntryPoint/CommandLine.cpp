// CommandLine (CommandLine.hpp).

#include "CommandLine.hpp"

#include <charconv>
#include <utility>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <shellapi.h>
#include <windows.h>
#endif

namespace Runic::Application {

CommandLine::CommandLine(std::vector<std::string> arguments) : m_Arguments(std::move(arguments)) {}

CommandLine CommandLine::FromMain(int argc, char **argv) {
    std::vector<std::string> arguments;
#if defined(_WIN32)
    (void)argc;
    (void)argv;
    int count = 0;
    LPWSTR *wide = CommandLineToArgvW(GetCommandLineW(), &count);
    for (int i = 0; i < count; ++i) {
        const int bytes =
            WideCharToMultiByte(CP_UTF8, 0, wide[i], -1, nullptr, 0, nullptr, nullptr);
        std::string utf8(static_cast<std::size_t>(bytes > 0 ? bytes - 1 : 0), '\0');
        WideCharToMultiByte(CP_UTF8, 0, wide[i], -1, utf8.data(), bytes, nullptr, nullptr);
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
    for (std::size_t i = 1; i < m_Arguments.size(); ++i) {
        if (m_Arguments[i] == flag) {
            return true;
        }
    }
    return false;
}

std::optional<std::string_view> CommandLine::ValueOf(std::string_view flag) const noexcept {
    for (std::size_t i = 1; i < m_Arguments.size(); ++i) {
        const std::string_view argument = m_Arguments[i];
        if (argument == flag) {
            if (i + 1 < m_Arguments.size()) {
                return std::string_view(m_Arguments[i + 1]);
            }
            return std::nullopt;
        }
        if (argument.size() > flag.size() && argument.substr(0, flag.size()) == flag &&
            argument[flag.size()] == '=') {
            return argument.substr(flag.size() + 1);
        }
    }
    return std::nullopt;
}

std::optional<std::uint64_t> CommandLine::QuitAfterFrames() const noexcept {
    const std::optional<std::string_view> value = ValueOf("--quit-after");
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
