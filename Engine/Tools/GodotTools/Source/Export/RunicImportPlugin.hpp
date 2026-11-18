#pragma once

#include <godot_cpp/classes/editor_export_plugin.hpp>
#include <godot_cpp/classes/editor_scene_post_import_plugin.hpp>
#include <godot_cpp/variant/callable.hpp>

// The Godot side of the Cooker integration (GDT-4). The import plugin adds the collision preset to
// a scene's Import dock ("Runic › Collision") and, after Godot imported the file, puts the preset
// on each mesh node and hides the collision-only meshes of the naming convention (they stay in the
// imported scene for the cook, never drawn). The export plugin cooks the project when Godot
// exports it, before the .pck is written. Private to Runic::GodotTools.

namespace Runic::GodotTools::Detail {

class RunicImportPlugin : public godot::EditorScenePostImportPlugin {
    GDCLASS(RunicImportPlugin, godot::EditorScenePostImportPlugin)

  public:
    void _get_import_options(const godot::String &path) override;
    void _post_process(godot::Node *scene) override;

  protected:
    static void _bind_methods() {}
};

class RunicExportPlugin : public godot::EditorExportPlugin {
    GDCLASS(RunicExportPlugin, godot::EditorExportPlugin)

  public:
    // What cooks the project (the editor plugin's cook).
    void SetCook(const godot::Callable &cook) { m_Cook = cook; }
    godot::String _get_name() const override;
    void _export_begin(const godot::PackedStringArray &features, bool debug,
                       const godot::String &path, uint32_t flags) override;

  protected:
    static void _bind_methods() {}

  private:
    godot::Callable m_Cook;
};

} // namespace Runic::GodotTools::Detail
