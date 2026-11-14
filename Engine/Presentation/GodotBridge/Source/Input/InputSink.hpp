#pragma once

// The input sink (GDB-1): a node at the root of Godot's scene tree that receives only the input
// Godot's UI did not consume (_unhandled_input) and posts it to the LayerStack as AppEvents.
// Private to Runic::GodotBridge.

#include "AppEvent.hpp"
#include "Layer.hpp"

#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/classes/node.hpp>

#include <optional>

namespace Runic::GodotBridge::Detail {

class RunicInputSink : public godot::Node {
    GDCLASS(RunicInputSink, godot::Node)

  public:
    void SetContext(Application::LayerContext *context) { m_Context = context; }
    void _unhandled_input(const godot::Ref<godot::InputEvent> &event) override;
    void _notification(int what);
    void OnRootResized();

  protected:
    static void _bind_methods() {}

  private:
    void Post(const Application::AppEvent &event);

    Application::LayerContext *m_Context = nullptr;
};

// Registers RunicInputSink with Godot; called once at the Scene initialization level.
void RegisterInputSink();
// Adds a sink under the scene tree's root and connects the root window's resize; nullptr when there
// is no scene tree.
RunicInputSink *AttachInputSink(Application::LayerContext &context);
// The AppEvent for an input event, or nothing for kinds the stack does not take (joypads, touch,
// gestures).
std::optional<Application::AppEvent> ToAppEvent(const godot::Ref<godot::InputEvent> &event);
// Pushes a synthetic left-button event or key event into the root viewport, as the window would.
void PushClick(float x, float y, bool pressed);
void PushKey(std::uint32_t key, bool pressed);

} // namespace Runic::GodotBridge::Detail
