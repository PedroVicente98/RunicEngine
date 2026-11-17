// A game editor library for GDT-2's checks: built twice, with RUNIC_FIXTURE_BUILD 1 and 2, so a
// test can replace one with the other under the RunicEditor and see the reload. It registers
// RunicFixtureNode, whose build_id() says which build is loaded, and prints a line when it loads.

#include <gdextension_interface.h>

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/defs.hpp>
#include <godot_cpp/godot.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#ifndef RUNIC_FIXTURE_BUILD
#error "RUNIC_FIXTURE_BUILD names the build"
#endif

namespace {

class RunicFixtureNode : public godot::Node {
    GDCLASS(RunicFixtureNode, godot::Node)

  public:
    int64_t build_id() const { return RUNIC_FIXTURE_BUILD; }

  protected:
    static void _bind_methods() {
        godot::ClassDB::bind_method(godot::D_METHOD("build_id"), &RunicFixtureNode::build_id);
    }
};

void Initialize(godot::ModuleInitializationLevel level) {
    if (level != godot::MODULE_INITIALIZATION_LEVEL_SCENE) {
        return;
    }
    GDREGISTER_CLASS(RunicFixtureNode);
    godot::UtilityFunctions::print("RUNIC: game editor fixture loaded (build ", RUNIC_FIXTURE_BUILD,
                                   ")");
}

void Uninitialize(godot::ModuleInitializationLevel) {}

} // namespace

extern "C" GDExtensionBool GDE_EXPORT
game_editor_init(GDExtensionInterfaceGetProcAddress getProcAddress,
                 GDExtensionClassLibraryPtr library, GDExtensionInitialization *initialization) {
    godot::GDExtensionBinding::InitObject init(getProcAddress, library, initialization);
    init.register_initializer(Initialize);
    init.register_terminator(Uninitialize);
    init.set_minimum_library_initialization_level(godot::MODULE_INITIALIZATION_LEVEL_SCENE);
    return init.init();
}
