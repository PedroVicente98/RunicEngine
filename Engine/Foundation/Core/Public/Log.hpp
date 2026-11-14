#pragma once

#include <cstddef>
#include <cstdint>
#include <format>
#include <string_view>
#include <utility>

// Logging hooks: Runic code formats a line and hands it to one process-wide sink that the host
// installs (APP-1's entry point writes to stdout and stderr; tests capture). Core decides no I/O
// beyond the default sink, which writes to stderr so that nothing logged before a host installs its
// own sink is lost. Lines use std::format syntax and are formatted into a stack buffer, so a line
// never allocates; a longer line is cut and ends with "...".

namespace Runic::Core {

enum class LogLevel : std::uint8_t { Trace, Debug, Info, Warning, Error, Fatal };

// One line as the sink receives it; message is valid only during the call.
struct LogRecord {
    LogLevel level;
    std::string_view message;
    const char *file; // __FILE__ of the call
    int line;
};

// Receives every line at or above the minimum level, on the thread that logged it, so a sink that
// several threads use must be thread-safe. user is the pointer given to SetLogSink.
using LogSink = void (*)(const LogRecord &record, void *user);

// Installs the sink; nullptr restores the default, which writes "<Level>: <message>" lines to
// stderr. Call it at start, before other threads log; the sink and user must outlive every log
// call.
void SetLogSink(LogSink sink, void *user) noexcept;

// Lines below the minimum level are dropped before they are formatted. The default is Info.
// Thread-safe.
void SetMinimumLogLevel(LogLevel level) noexcept;
LogLevel MinimumLogLevel() noexcept;
bool IsLogLevelEnabled(LogLevel level) noexcept;

// The longest line a sink receives, in bytes.
inline constexpr std::size_t kMaxLogLineBytes = 512;

// Hands an already formatted line to the sink (lines from another system, such as Godot's output),
// cut to kMaxLogLineBytes like a formatted one.
void WriteLog(LogLevel level, std::string_view message, const char *file, int line) noexcept;

namespace Detail {
// Hands the line Log formatted into buffer (kMaxLogLineBytes long) to the sink. formattedSize is
// the size the whole line needed: when it exceeds the buffer, the line is cut and its last three
// bytes become "...".
void WriteFormattedLog(LogLevel level, char *buffer, std::size_t formattedSize, const char *file,
                       int line) noexcept;
} // namespace Detail

// Formats with std::format syntax and writes the line, with no allocation. Use the RUNIC_LOG_*
// macros.
template <class... Args>
void Log(LogLevel level, const char *file, int line, std::format_string<Args...> format,
         Args &&...args) {
    if (!IsLogLevelEnabled(level)) {
        return;
    }
    char buffer[kMaxLogLineBytes];
    const auto result = std::format_to_n(buffer, static_cast<std::ptrdiff_t>(kMaxLogLineBytes),
                                         format, std::forward<Args>(args)...);
    Detail::WriteFormattedLog(level, buffer, static_cast<std::size_t>(result.size), file, line);
}

} // namespace Runic::Core

#define RUNIC_LOG(level, ...) ::Runic::Core::Log(level, __FILE__, __LINE__, __VA_ARGS__)
#define RUNIC_LOG_TRACE(...) RUNIC_LOG(::Runic::Core::LogLevel::Trace, __VA_ARGS__)
#define RUNIC_LOG_DEBUG(...) RUNIC_LOG(::Runic::Core::LogLevel::Debug, __VA_ARGS__)
#define RUNIC_LOG_INFO(...) RUNIC_LOG(::Runic::Core::LogLevel::Info, __VA_ARGS__)
#define RUNIC_LOG_WARNING(...) RUNIC_LOG(::Runic::Core::LogLevel::Warning, __VA_ARGS__)
#define RUNIC_LOG_ERROR(...) RUNIC_LOG(::Runic::Core::LogLevel::Error, __VA_ARGS__)
#define RUNIC_LOG_FATAL(...) RUNIC_LOG(::Runic::Core::LogLevel::Fatal, __VA_ARGS__)
