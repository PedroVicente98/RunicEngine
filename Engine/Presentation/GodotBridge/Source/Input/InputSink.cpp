// The input sink (InputSink.hpp): Godot's unhandled input becomes AppEvents.

#include "Input/InputSink.hpp"

#include "GodotLayer/SceneRoot.hpp"

#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/input_event_key.hpp>
#include <godot_cpp/classes/input_event_mouse_button.hpp>
#include <godot_cpp/classes/input_event_mouse_motion.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/window.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>

namespace Runic::GodotBridge::Detail {

namespace {

using namespace Runic::Application;

// The sink's node name in Godot's scene tree, and the root window's signal it listens to.
constexpr const char *kInputSinkNodeName = "RunicInputSink";
constexpr const char *kRootResizedSignal = "size_changed";
// The wheel step when Godot reports no amount (its factor is 0 on platforms without one).
constexpr float kDefaultWheelStep = 1.0f;

godot::Window *Root() { return SceneRoot(); }

std::uint32_t Modifiers(const godot::InputEventWithModifiers &event) {
    return (event.is_shift_pressed() ? kModifierShift : kNoModifiers) |
           (event.is_ctrl_pressed() ? kModifierControl : kNoModifiers) |
           (event.is_alt_pressed() ? kModifierAlt : kNoModifiers) |
           (event.is_meta_pressed() ? kModifierMeta : kNoModifiers);
}

std::optional<MouseButton> ToButton(godot::MouseButton button) {
    switch (button) {
    case godot::MOUSE_BUTTON_LEFT:
        return MouseButton::Left;
    case godot::MOUSE_BUTTON_RIGHT:
        return MouseButton::Right;
    case godot::MOUSE_BUTTON_MIDDLE:
        return MouseButton::Middle;
    case godot::MOUSE_BUTTON_XBUTTON1:
        return MouseButton::Back;
    case godot::MOUSE_BUTTON_XBUTTON2:
        return MouseButton::Forward;
    default:
        return std::nullopt;
    }
}

} // namespace

std::optional<AppEvent> ToAppEvent(const godot::Ref<godot::InputEvent> &event) {
    if (event.is_null()) {
        return std::nullopt;
    }
    if (const auto *key = godot::Object::cast_to<godot::InputEventKey>(event.ptr())) {
        godot::Key code = key->get_keycode();
        if (code == godot::KEY_NONE) {
            code = key->get_physical_keycode();
        }
        return AppEvent{KeyEvent{static_cast<std::uint32_t>(code), Modifiers(*key),
                                 key->is_pressed(), key->is_echo()}};
    }
    if (const auto *mouse = godot::Object::cast_to<godot::InputEventMouseButton>(event.ptr())) {
        const godot::Vector2 position = mouse->get_position();
        const godot::MouseButton index = mouse->get_button_index();
        if (index == godot::MOUSE_BUTTON_WHEEL_UP || index == godot::MOUSE_BUTTON_WHEEL_DOWN ||
            index == godot::MOUSE_BUTTON_WHEEL_LEFT || index == godot::MOUSE_BUTTON_WHEEL_RIGHT) {
            if (!mouse->is_pressed()) {
                return std::nullopt; // a wheel step arrives as a press and a release; one event is
                                     // enough
            }
            const float factor = mouse->get_factor();
            const float step = factor > 0.0f ? factor : kDefaultWheelStep;
            MouseWheelEvent wheel{.x = position.x, .y = position.y};
            if (index == godot::MOUSE_BUTTON_WHEEL_UP || index == godot::MOUSE_BUTTON_WHEEL_DOWN) {
                wheel.deltaY = index == godot::MOUSE_BUTTON_WHEEL_UP ? step : -step;
            } else {
                wheel.deltaX = index == godot::MOUSE_BUTTON_WHEEL_RIGHT ? step : -step;
            }
            return AppEvent{wheel};
        }
        const std::optional<MouseButton> button = ToButton(index);
        if (!button) {
            return std::nullopt;
        }
        return AppEvent{MouseButtonEvent{*button, mouse->is_pressed(), position.x, position.y,
                                         Modifiers(*mouse)}};
    }
    if (const auto *motion = godot::Object::cast_to<godot::InputEventMouseMotion>(event.ptr())) {
        const godot::Vector2 position = motion->get_position();
        const godot::Vector2 relative = motion->get_relative();
        return AppEvent{MouseMoveEvent{position.x, position.y, relative.x, relative.y}};
    }
    return std::nullopt;
}

void RunicInputSink::_unhandled_input(const godot::Ref<godot::InputEvent> &event) {
    if (std::optional<AppEvent> converted = ToAppEvent(event)) {
        Post(*converted);
    }
}

void RunicInputSink::_notification(int what) {
    if (what == NOTIFICATION_APPLICATION_FOCUS_IN || what == NOTIFICATION_APPLICATION_FOCUS_OUT) {
        Post(AppEvent{WindowFocusEvent{what == NOTIFICATION_APPLICATION_FOCUS_IN}});
    }
}

void RunicInputSink::OnRootResized() {
    if (godot::Window *root = Root()) {
        const godot::Vector2i size = root->get_size();
        Post(AppEvent{WindowResizeEvent{static_cast<std::uint32_t>(size.x),
                                        static_cast<std::uint32_t>(size.y)}});
    }
}

void RunicInputSink::Post(const AppEvent &event) {
    if (m_Context != nullptr) {
        m_Context->PostEvent(event);
    }
}

void RegisterInputSink() { godot::ClassDB::register_internal_class<RunicInputSink>(); }

RunicInputSink *AttachInputSink(LayerContext &context) {
    godot::Window *root = Root();
    if (root == nullptr) {
        return nullptr;
    }
    auto *sink = memnew(RunicInputSink);
    sink->set_name(kInputSinkNodeName);
    sink->SetContext(&context);
    sink->set_process_unhandled_input(true);
    root->add_child(sink);
    root->connect(kRootResizedSignal, callable_mp(sink, &RunicInputSink::OnRootResized));
    return sink;
}

void PushClick(float x, float y, bool pressed) {
    godot::Window *root = Root();
    if (root == nullptr) {
        return;
    }
    godot::Ref<godot::InputEventMouseButton> event;
    event.instantiate();
    event->set_button_index(godot::MOUSE_BUTTON_LEFT);
    event->set_pressed(pressed);
    event->set_position(godot::Vector2(x, y));
    event->set_global_position(godot::Vector2(x, y));
    root->push_input(event);
}

void PushKey(std::uint32_t key, bool pressed) {
    godot::Window *root = Root();
    if (root == nullptr) {
        return;
    }
    godot::Ref<godot::InputEventKey> event;
    event.instantiate();
    event->set_keycode(static_cast<godot::Key>(key));
    event->set_physical_keycode(static_cast<godot::Key>(key));
    event->set_pressed(pressed);
    root->push_input(event);
}

} // namespace Runic::GodotBridge::Detail
