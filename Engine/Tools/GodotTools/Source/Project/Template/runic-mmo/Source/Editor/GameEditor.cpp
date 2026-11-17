// The game's editor library: one example marker node, registered when the RunicEditor loads the library.
// Add the game's marker nodes and docks here; rebuild and the editor reloads them.

#include <gdextension_interface.h>

#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/defs.hpp>
#include <godot_cpp/godot.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace {

// An example marker: a place in a scene the game's tools know about.
class GameMarker3D : public godot::Node3D {
    GDCLASS(GameMarker3D, godot::Node3D)

  protected:
    static void _bind_methods() {}
};

void Initialize(godot::ModuleInitializationLevel level) {
    if (level != godot::MODULE_INITIALIZATION_LEVEL_SCENE) {
        return;
    }
    GDREGISTER_CLASS(GameMarker3D);
    godot::UtilityFunctions::print("RUNIC: game editor library loaded");
}

void Uninitialize(godot::ModuleInitializationLevel) {}

} // namespace

extern "C" GDExtensionBool GDE_EXPORT game_editor_init(GDExtensionInterfaceGetProcAddress getProcAddress,
                                                       GDExtensionClassLibraryPtr library,
                                                       GDExtensionInitialization *initialization) {
    godot::GDExtensionBinding::InitObject init(getProcAddress, library, initialization);
    init.register_initializer(Initialize);
    init.register_terminator(Uninitialize);
    init.set_minimum_library_initialization_level(godot::MODULE_INITIALIZATION_LEVEL_SCENE);
    return init.init();
}
