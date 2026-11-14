// Logging hooks (Log.hpp): one process-wide sink, set at start; the default writes to stderr.

#include "Log.hpp"

#include <atomic>
#include <cstdio>
#include <cstring>

namespace Runic::Core {

namespace {

// Set at start, before other threads log (Log.hpp), so plain variables are enough for the sink.
LogSink g_Sink = nullptr;
void *g_SinkUser = nullptr;
std::atomic<LogLevel> g_MinimumLevel{LogLevel::Info};

constexpr const char *kEllipsis = "...";
constexpr std::size_t kEllipsisBytes = 3;

const char *LevelName(LogLevel level) noexcept {
    switch (level) {
    case LogLevel::Trace:
        return "Trace";
    case LogLevel::Debug:
        return "Debug";
    case LogLevel::Info:
        return "Info";
    case LogLevel::Warning:
        return "Warning";
    case LogLevel::Error:
        return "Error";
    case LogLevel::Fatal:
        return "Fatal";
    }
    return "Log";
}

void DefaultSink(const LogRecord &record, void *) {
    std::fprintf(stderr, "%s: %.*s\n", LevelName(record.level),
                 static_cast<int>(record.message.size()), record.message.data());
}

void Dispatch(LogLevel level, std::string_view message, const char *file, int line) noexcept {
    const LogRecord record{level, message, file, line};
    if (g_Sink != nullptr) {
        g_Sink(record, g_SinkUser);
    } else {
        DefaultSink(record, nullptr);
    }
}

} // namespace

void SetLogSink(LogSink sink, void *user) noexcept {
    g_Sink = sink;
    g_SinkUser = sink != nullptr ? user : nullptr;
}

void SetMinimumLogLevel(LogLevel level) noexcept {
    g_MinimumLevel.store(level, std::memory_order_relaxed);
}

LogLevel MinimumLogLevel() noexcept { return g_MinimumLevel.load(std::memory_order_relaxed); }

bool IsLogLevelEnabled(LogLevel level) noexcept { return level >= MinimumLogLevel(); }

void WriteLog(LogLevel level, std::string_view message, const char *file, int line) noexcept {
    if (!IsLogLevelEnabled(level)) {
        return;
    }
    if (message.size() <= kMaxLogLineBytes) {
        Dispatch(level, message, file, line);
        return;
    }
    char buffer[kMaxLogLineBytes];
    std::memcpy(buffer, message.data(), kMaxLogLineBytes - kEllipsisBytes);
    std::memcpy(buffer + kMaxLogLineBytes - kEllipsisBytes, kEllipsis, kEllipsisBytes);
    Dispatch(level, std::string_view(buffer, kMaxLogLineBytes), file, line);
}

namespace Detail {

void WriteFormattedLog(LogLevel level, char *buffer, std::size_t formattedSize, const char *file,
                       int line) noexcept {
    std::size_t size = formattedSize;
    if (size > kMaxLogLineBytes) {
        std::memcpy(buffer + kMaxLogLineBytes - kEllipsisBytes, kEllipsis, kEllipsisBytes);
        size = kMaxLogLineBytes;
    }
    Dispatch(level, std::string_view(buffer, size), file, line);
}

} // namespace Detail

} // namespace Runic::Core
