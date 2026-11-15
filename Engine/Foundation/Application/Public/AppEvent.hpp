#pragma once

#include <cstdint>
#include <variant>

// Application events (RunicDocs › Repositories, build targets and processes › Three event
// families): the input and window events the window layer (the GodotLayer) posts after Godot's UI
// took what it consumes, and a quit request. They are plain values with no strings, so posting and
// dispatching one never allocates.

namespace Runic::Application {

// Modifier keys held during an input event, as bits of KeyEvent::modifiers and
// MouseButtonEvent::modifiers.
inline constexpr std::uint32_t kNoModifiers = 0;
inline constexpr std::uint32_t kModifierShift = 1u << 0;
inline constexpr std::uint32_t kModifierControl = 1u << 1;
inline constexpr std::uint32_t kModifierAlt = 1u << 2;
inline constexpr std::uint32_t kModifierMeta = 1u << 3;

// The key code of no key (Godot's KEY_NONE).
inline constexpr std::uint32_t kNoKey = 0;

// Each event type below names its kind in kName, for logs (AppEventName).

// A key went down or up. key is the window layer's key code: GodotBridge passes Godot's key
// numbering (Unicode code points for printable keys, a reserved range for the others); input
// mapping (CLI-2) turns codes into actions.
struct KeyEvent {
    static constexpr const char *kName = "Key";
    std::uint32_t key = kNoKey;
    std::uint32_t modifiers = kNoModifiers;
    bool pressed = false;
    bool repeat = false;
};

enum class MouseButton : std::uint8_t { Left, Right, Middle, Back, Forward };

// A mouse button went down or up at a position in window pixels.
struct MouseButtonEvent {
    static constexpr const char *kName = "MouseButton";
    MouseButton button = MouseButton::Left;
    bool pressed = false;
    float x = 0.0f;
    float y = 0.0f;
    std::uint32_t modifiers = kNoModifiers;
};

struct MouseMoveEvent {
    static constexpr const char *kName = "MouseMove";
    float x = 0.0f;
    float y = 0.0f;
    float deltaX = 0.0f;
    float deltaY = 0.0f;
};

// A wheel or trackpad scroll; a positive deltaY scrolls up.
struct MouseWheelEvent {
    static constexpr const char *kName = "MouseWheel";
    float deltaX = 0.0f;
    float deltaY = 0.0f;
    float x = 0.0f;
    float y = 0.0f;
};

struct WindowResizeEvent {
    static constexpr const char *kName = "WindowResize";
    std::uint32_t width = 0;
    std::uint32_t height = 0;
};

struct WindowFocusEvent {
    static constexpr const char *kName = "WindowFocus";
    bool focused = false;
};

// The window was asked to close. When no layer handles it, the host stops with kCleanExit.
struct QuitRequestEvent {
    static constexpr const char *kName = "QuitRequest";
};

using AppEventData = std::variant<KeyEvent, MouseButtonEvent, MouseMoveEvent, MouseWheelEvent,
                                  WindowResizeEvent, WindowFocusEvent, QuitRequestEvent>;

// One event, dispatched top-down until a layer marks it handled.
struct AppEvent {
    AppEventData data;
    bool handled = false;

    template <class T> bool Is() const noexcept { return std::holds_alternative<T>(data); }
    template <class T> const T *As() const noexcept { return std::get_if<T>(&data); }
};

// The event's kind, for logs: the kName of the type it holds ("Key", "MouseButton", ...).
const char *AppEventName(const AppEvent &event) noexcept;

} // namespace Runic::Application
