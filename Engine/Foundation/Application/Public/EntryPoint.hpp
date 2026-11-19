#pragma once

#include <memory>

#include "Application.hpp"
#include "CommandLine.hpp"

// The Engine's entry point (RunicDocs › Application and LayerStack › Entry point): the Engine owns
// main(), and each executable defines CreateApplication. This header only declares, so any source
// file may include it. An executable expands RUNIC_ENTRY_POINT() once, in the source file where it
// defines CreateApplication: that is where main() is defined, and it hands over to the Engine
// (Detail::RunMain). A function defined in a header would be defined again in every source file
// that includes it; main() defined twice in one executable stops the link.

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

// Defines the executable's main(): expand it once, at namespace scope, with no semicolon after it.
#define RUNIC_ENTRY_POINT()                                                                        \
    int main(int argc, char **argv) { return ::Runic::Application::Detail::RunMain(argc, argv); }
