#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Runic::Application {

// The process's command line as UTF-8 on every platform: argv[0] first and every argument
// unchanged, because Godot's project manager relaunches the executable with its own arguments.
class CommandLine {
  public:
    CommandLine() = default;
    explicit CommandLine(std::vector<std::string> arguments);
    // From main()'s arguments; on Windows from the wide command line, since argv is in the ANSI
    // code page there.
    static CommandLine FromMain(int argc, char **argv);

    const std::vector<std::string> &Arguments() const noexcept;
    // Whether a flag such as "--headless" appears after argv[0].
    bool Has(std::string_view flag) const noexcept;
    // The value of "--flag value" or "--flag=value"; empty when the flag is absent or has no value.
    std::optional<std::string_view> ValueOf(std::string_view flag) const noexcept;
    // --quit-after N, the frame limit Godot reads too; empty when absent or not a number.
    std::optional<std::uint64_t> QuitAfterFrames() const noexcept;

  private:
    std::vector<std::string> m_Arguments;
};

} // namespace Runic::Application
