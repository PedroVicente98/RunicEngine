// The body of the Engine's main(), which RUNIC_ENTRY_POINT() defines (EntryPoint.hpp): the console
// log sink, the stop signals and the host.

#include "EntryPoint.hpp"

#include "ApplicationHost.hpp"
#include "Log.hpp"

#include <atomic>
#include <csignal>
#include <cstdio>
#include <utility>

namespace Runic::Application::Detail {

namespace {

std::atomic<ApplicationHost *> g_Host{nullptr};

// SIGINT and SIGTERM ask for a stop at the next safe point; only atomics are touched here.
void OnStopSignal(int) {
    if (ApplicationHost *host = g_Host.load()) {
        host->RequestStop(0);
    }
}

// Info and below to stdout as they are; warnings and errors to stderr with their level.
void ConsoleSink(const Core::LogRecord &record, void *) {
    const int size = static_cast<int>(record.message.size());
    switch (record.level) {
    case Core::LogLevel::Warning:
        std::fprintf(stderr, "Warning: %.*s\n", size, record.message.data());
        break;
    case Core::LogLevel::Error:
        std::fprintf(stderr, "Error: %.*s\n", size, record.message.data());
        break;
    case Core::LogLevel::Fatal:
        std::fprintf(stderr, "Fatal: %.*s\n", size, record.message.data());
        break;
    default:
        std::fprintf(stdout, "%.*s\n", size, record.message.data());
        std::fflush(stdout);
        break;
    }
}

} // namespace

int RunMain(int argc, char **argv) {
    Core::SetLogSink(&ConsoleSink, nullptr);
    CommandLine command = CommandLine::FromMain(argc, argv);
    std::unique_ptr<Application> application = CreateApplication(command);
    if (application == nullptr) {
        return 0;
    }
    ApplicationHost host(std::move(application), std::move(command));
    g_Host.store(&host);
    std::signal(SIGINT, OnStopSignal);
    std::signal(SIGTERM, OnStopSignal);
    const int exitCode = host.Run();
    std::signal(SIGINT, SIG_DFL);
    std::signal(SIGTERM, SIG_DFL);
    g_Host.store(nullptr);
    return exitCode;
}

} // namespace Runic::Application::Detail
