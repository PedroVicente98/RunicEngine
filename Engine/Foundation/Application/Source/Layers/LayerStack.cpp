// LayerStack (LayerStack.hpp): two ranges and a queue of changes applied only at the safe point.

#include "LayerStack.hpp"

#include "Log.hpp"

#include <algorithm>
#include <utility>

namespace Runic::Application {

struct LayerStack::Change {
    enum class Kind { PushLayer, PushOverlay, Pop, Replace };

    Kind kind;
    std::unique_ptr<Layer> layer;  // the new layer of a push or a replacement
    const Layer *target = nullptr; // the layer to pop or replace
};

namespace {

using Range = std::vector<std::unique_ptr<Layer>>;

// The range and position of a layer, or a null range when the stack does not hold it.
std::pair<Range *, Range::iterator> Find(Range &layers, Range &overlays, const Layer *target) {
    for (Range *range : {&layers, &overlays}) {
        const auto it = std::find_if(
            range->begin(), range->end(),
            [target](const std::unique_ptr<Layer> &layer) { return layer.get() == target; });
        if (it != range->end()) {
            return {range, it};
        }
    }
    return {nullptr, layers.end()};
}

Core::Result<void> Attach(Layer &layer, LayerContext &context) {
    Core::Result<void> attached = layer.OnAttach(context);
    if (!attached) {
        return Core::Fail(layer.Name() + " failed to attach: " + attached.GetError().message);
    }
    return {};
}

} // namespace

LayerStack::LayerStack() = default;
LayerStack::~LayerStack() = default;

void LayerStack::PushLayer(std::unique_ptr<Layer> layer) {
    m_Pending.push_back({Change::Kind::PushLayer, std::move(layer), nullptr});
}

void LayerStack::PushOverlay(std::unique_ptr<Layer> overlay) {
    m_Pending.push_back({Change::Kind::PushOverlay, std::move(overlay), nullptr});
}

void LayerStack::PopLayer(const Layer &layer) {
    m_Pending.push_back({Change::Kind::Pop, nullptr, &layer});
}

void LayerStack::ReplaceLayer(const Layer &current, std::unique_ptr<Layer> replacement) {
    m_Pending.push_back({Change::Kind::Replace, std::move(replacement), &current});
}

bool LayerStack::HasPendingChanges() const noexcept { return !m_Pending.empty(); }

Core::Result<void> LayerStack::ApplyPendingChanges(LayerContext &context) {
    std::vector<Change> changes = std::move(m_Pending);
    m_Pending.clear();
    Core::Result<void> result = {};
    const auto record = [&result](Core::Result<void> attempt) {
        if (!attempt && result) {
            result = std::move(attempt);
        }
    };
    for (Change &change : changes) {
        switch (change.kind) {
        case Change::Kind::PushLayer:
        case Change::Kind::PushOverlay: {
            Core::Result<void> attached = Attach(*change.layer, context);
            if (attached) {
                (change.kind == Change::Kind::PushLayer ? m_Layers : m_Overlays)
                    .push_back(std::move(change.layer));
            }
            record(std::move(attached));
            break;
        }
        case Change::Kind::Pop: {
            const auto [range, it] = Find(m_Layers, m_Overlays, change.target);
            if (range == nullptr) {
                RUNIC_LOG_WARNING("LayerStack: a pop names a layer the stack does not hold");
                break;
            }
            (*it)->OnDetach();
            range->erase(it);
            break;
        }
        case Change::Kind::Replace: {
            const auto [range, it] = Find(m_Layers, m_Overlays, change.target);
            if (range == nullptr) {
                RUNIC_LOG_WARNING(
                    "LayerStack: a replacement names a layer the stack does not hold");
                break;
            }
            (*it)->OnDetach(); // the old layer releases its resources before the new one acquires
                               // them
            Core::Result<void> attached = Attach(*change.layer, context);
            if (attached) {
                *it = std::move(change.layer);
            } else {
                range->erase(it);
            }
            record(std::move(attached));
            break;
        }
        }
    }
    return result;
}

void LayerStack::DispatchEvent(AppEvent &event) {
    for (auto it = m_Overlays.rbegin(); it != m_Overlays.rend() && !event.handled; ++it) {
        (*it)->OnEvent(event);
    }
    for (auto it = m_Layers.rbegin(); it != m_Layers.rend() && !event.handled; ++it) {
        (*it)->OnEvent(event);
    }
}

void LayerStack::UpdateBottomUp(const FrameTime &time) {
    for (const std::unique_ptr<Layer> &layer : m_Layers) {
        layer->OnUpdate(time);
    }
    for (const std::unique_ptr<Layer> &overlay : m_Overlays) {
        overlay->OnUpdate(time);
    }
}

void LayerStack::RenderBottomUp() {
    for (const std::unique_ptr<Layer> &layer : m_Layers) {
        layer->OnRender();
    }
    for (const std::unique_ptr<Layer> &overlay : m_Overlays) {
        overlay->OnRender();
    }
}

void LayerStack::DetachAll() {
    for (Range *range : {&m_Overlays, &m_Layers}) {
        while (!range->empty()) {
            range->back()->OnDetach();
            range->pop_back();
        }
    }
    m_Pending.clear(); // layers that never attached are destroyed without a detach
}

std::size_t LayerStack::LayerCount() const noexcept { return m_Layers.size(); }
std::size_t LayerStack::OverlayCount() const noexcept { return m_Overlays.size(); }

std::vector<std::string> LayerStack::Names() const {
    std::vector<std::string> names;
    for (const Range *range : {&m_Layers, &m_Overlays}) {
        for (const std::unique_ptr<Layer> &layer : *range) {
            names.push_back(layer->Name());
        }
    }
    return names;
}

} // namespace Runic::Application
