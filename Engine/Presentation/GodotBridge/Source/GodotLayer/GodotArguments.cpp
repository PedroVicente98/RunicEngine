// Godot's command-line flags (GodotArguments.hpp).

#include "GodotArguments.hpp"

#include <iterator>

namespace Runic::GodotBridge {

void InsertProjectPath(std::vector<std::string> &arguments, std::string_view folder) {
    // After argv[0], the executable's path; an empty list gets the flag first.
    const auto afterExecutable = arguments.empty() ? arguments.end() : std::next(arguments.begin());
    arguments.insert(afterExecutable, {std::string(kPathFlag), std::string(folder)});
}

} // namespace Runic::GodotBridge
