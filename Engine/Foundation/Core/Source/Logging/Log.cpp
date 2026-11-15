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

// What ends a line cut at kMaxLogLineBytes.
constexpr std::string_view kEllipsis = "...";

void DefaultSink(const LogRecord &record, void *) {
    const std::string_view level = LogLevelName(record.level);
    std::fprintf(stderr, "%.*s: %.*s\n", static_cast<int>(level.size()), level.data(),
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

std::string_view LogLevelName(LogLevel level) noexcept {
    switch (level) {
    case LogLevel::Trace:
        return LogLevelNames::kTrace;
    case LogLevel::Debug:
        return LogLevelNames::kDebug;
    case LogLevel::Info:
        return LogLevelNames::kInfo;
    case LogLevel::Warning:
        return LogLevelNames::kWarning;
    case LogLevel::Error:
        return LogLevelNames::kError;
    case LogLevel::Fatal:
        return LogLevelNames::kFatal;
    }
    return LogLevelNames::kUnknown;
}

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
    std::memcpy(buffer, message.data(), kMaxLogLineBytes - kEllipsis.size());
    std::memcpy(buffer + kMaxLogLineBytes - kEllipsis.size(), kEllipsis.data(), kEllipsis.size());
    Dispatch(level, std::string_view(buffer, kMaxLogLineBytes), file, line);
}

namespace Detail {

void WriteFormattedLog(LogLevel level, char *buffer, std::size_t formattedSize, const char *file,
                       int line) noexcept {
    std::size_t size = formattedSize;
    if (size > kMaxLogLineBytes) {
        std::memcpy(buffer + kMaxLogLineBytes - kEllipsis.size(), kEllipsis.data(),
                    kEllipsis.size());
        size = kMaxLogLineBytes;
    }
    Dispatch(level, std::string_view(buffer, size), file, line);
}

} // namespace Detail

} // namespace Runic::Core
