#pragma once

#include "GodotToolsModule.hpp"

// What RegisterEditorTools was given, for the classes Godot makes later (the plugin). One editor
// per process, so one context. Private to Runic::GodotTools.

namespace Runic::GodotTools::Detail {

const EditorToolsSpec &Context() noexcept;
void SetContext(const EditorToolsSpec &spec);

} // namespace Runic::GodotTools::Detail
