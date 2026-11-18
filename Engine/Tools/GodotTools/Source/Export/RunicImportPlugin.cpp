// The import and export plugins (RunicImportPlugin.hpp).

#include "Export/RunicImportPlugin.hpp"

#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/variant/string_name.hpp>

#include "EditorCook.hpp"
#include "Log.hpp"

namespace Runic::GodotTools::Detail {

namespace {

constexpr const char *kExportPluginName = "Runic";

godot::String ToGodot(std::string_view text) {
    return godot::String::utf8(text.data(), static_cast<int64_t>(text.size()));
}

bool IsCollisionOnly(const godot::String &name) {
    return name.contains(ToGodot(kCollisionOnlySuffix));
}

void Mark(godot::Node *node, int64_t preset) {
    if (auto *mesh = godot::Object::cast_to<godot::MeshInstance3D>(node)) {
        mesh->set_meta(ToGodot(kCollisionPresetMeta), preset);
        if (IsCollisionOnly(mesh->get_name())) {
            mesh->set_meta(ToGodot(kCollisionOnlyMeta), true);
            mesh->set_visible(false);
        }
    }
    for (int32_t i = 0; i < node->get_child_count(); ++i) {
        Mark(node->get_child(i), preset);
    }
}

} // namespace

void RunicImportPlugin::_get_import_options(const godot::String &) {
    add_import_option_advanced(godot::Variant::INT, ToGodot(kCollisionPresetOption),
                               static_cast<int64_t>(CollisionPreset::MeshItself),
                               godot::PROPERTY_HINT_ENUM, ToGodot(CollisionPresetHint()));
}

void RunicImportPlugin::_post_process(godot::Node *scene) {
    if (scene == nullptr) {
        return;
    }
    const godot::Variant option = get_option_value(ToGodot(kCollisionPresetOption));
    const int64_t preset = option.get_type() == godot::Variant::INT
                               ? static_cast<int64_t>(option)
                               : static_cast<int64_t>(CollisionPreset::MeshItself);
    Mark(scene, preset);
}

godot::String RunicExportPlugin::_get_name() const { return kExportPluginName; }

void RunicExportPlugin::_export_begin(const godot::PackedStringArray &, bool,
                                      const godot::String &path, uint32_t) {
    RUNIC_LOG_INFO("RUNIC: exporting {}: the project cooks first", path.utf8().get_data());
    if (m_Cook.is_valid()) {
        m_Cook.call();
    }
}

} // namespace Runic::GodotTools::Detail
