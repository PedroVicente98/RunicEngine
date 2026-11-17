// The Input layer (InputLayer.hpp): AppEvents into the InputMap, then the command builders.

#include "InputLayer.hpp"

#include <utility>

#include "Assert.hpp"

namespace Runic::Client {

InputLayer::InputLayer() : Layer(std::string(kInputLayerName)) {}

InputLayer::~InputLayer() = default;

void InputLayer::AddCommandBuilder(std::string_view name, CommandBuilder builder) {
    RUNIC_ASSERT(builder != nullptr);
    m_Builders.push_back({std::string(name), std::move(builder)});
}

void InputLayer::OnEvent(Application::AppEvent &event) {
    if (event.handled) {
        return; // a layer above (Godot's UI) kept it
    }
    if (const auto *focus = event.As<Application::WindowFocusEvent>();
        focus != nullptr && !focus->focused) {
        // The releases of keys held now may never come: let go of everything.
        m_Map.ReleaseAll();
        return;
    }
    m_Map.Feed(event);
}

void InputLayer::OnUpdate(const Application::FrameTime &time) {
    for (const Builder &builder : m_Builders) {
        builder.build(m_Map, time);
    }
    m_Map.EndFrame();
}

} // namespace Runic::Client
