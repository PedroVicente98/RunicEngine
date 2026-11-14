#pragma once

#include <memory>

#include "Application.hpp"
#include "CommandLine.hpp"

// The Engine's entry point (RunicDocs › Application and LayerStack › Entry point). An executable
// defines CreateApplication and includes this header in exactly one of its source files: the header
// defines main(), which hands over to the Engine (Detail::RunMain). main() is defined here rather
// than in Runic::Application's library so that a test executable can link the module without a
// second main().

namespace Runic::Application {

// Defined by each executable: builds its Application from the command line. Returning nullptr ends
// the process with exit code 0 before any layer runs (the RunicEditor refuses Godot's Play relaunch
// this way).
std::unique_ptr<Application> CreateApplication(const CommandLine &command);

namespace Detail {
// The body of main(): the console log sink (Info and below to stdout, warnings and errors to
// stderr), SIGINT and SIGTERM as a stop request, CreateApplication, then the host's Run.
int RunMain(int argc, char **argv);
} // namespace Detail

} // namespace Runic::Application

#if !defined(RUNIC_APPLICATION_NO_MAIN)
int main(int argc, char **argv) { return ::Runic::Application::Detail::RunMain(argc, argv); }
#endif
