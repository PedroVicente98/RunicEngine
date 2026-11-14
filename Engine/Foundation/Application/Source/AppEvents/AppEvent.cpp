// Application events (AppEvent.hpp).

#include "AppEvent.hpp"

#include <type_traits>

namespace Runic::Application {

const char *AppEventName(const AppEvent &event) noexcept {
    return std::visit(
        [](const auto &data) -> const char * {
            using T = std::decay_t<decltype(data)>;
            if constexpr (std::is_same_v<T, KeyEvent>) {
                return "Key";
            } else if constexpr (std::is_same_v<T, MouseButtonEvent>) {
                return "MouseButton";
            } else if constexpr (std::is_same_v<T, MouseMoveEvent>) {
                return "MouseMove";
            } else if constexpr (std::is_same_v<T, MouseWheelEvent>) {
                return "MouseWheel";
            } else if constexpr (std::is_same_v<T, WindowResizeEvent>) {
                return "WindowResize";
            } else if constexpr (std::is_same_v<T, WindowFocusEvent>) {
                return "WindowFocus";
            } else {
                return "QuitRequest";
            }
        },
        event.data);
}

} // namespace Runic::Application
