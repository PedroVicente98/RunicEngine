// Application (Application.hpp).

#include "Application.hpp"

#include <utility>

namespace Runic::Application {

Application::Application(ApplicationSpec spec) : m_Spec(std::move(spec)) {}
Application::~Application() = default;

const ApplicationSpec &Application::Spec() const noexcept { return m_Spec; }
void Application::PushLayer(std::unique_ptr<Layer> layer) { m_Layers.PushLayer(std::move(layer)); }
void Application::PushOverlay(std::unique_ptr<Layer> overlay) {
    m_Layers.PushOverlay(std::move(overlay));
}
LayerStack &Application::Layers() noexcept { return m_Layers; }
void Application::OnStopped(int, std::uint64_t) {}

} // namespace Runic::Application
