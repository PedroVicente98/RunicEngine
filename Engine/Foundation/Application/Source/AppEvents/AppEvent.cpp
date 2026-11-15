// Application events (AppEvent.hpp).

#include "AppEvent.hpp"

#include <type_traits>

namespace Runic::Application {

const char *AppEventName(const AppEvent &event) noexcept {
    return std::visit(
        [](const auto &data) -> const char * { return std::decay_t<decltype(data)>::kName; },
        event.data);
}

} // namespace Runic::Application
