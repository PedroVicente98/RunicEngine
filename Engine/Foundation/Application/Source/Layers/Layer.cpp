// Layer (Layer.hpp): every hook does nothing until a layer overrides it.

#include "Layer.hpp"

#include <utility>

namespace Runic::Application {

Layer::Layer(std::string name) : m_Name(std::move(name)) {}
Layer::~Layer() = default;

const std::string &Layer::Name() const noexcept { return m_Name; }

Core::Result<void> Layer::OnAttach(LayerContext &) { return {}; }
void Layer::OnDetach() {}
void Layer::OnEvent(AppEvent &) {}
void Layer::OnUpdate(const FrameTime &) {}
void Layer::OnRender() {}

} // namespace Runic::Application
