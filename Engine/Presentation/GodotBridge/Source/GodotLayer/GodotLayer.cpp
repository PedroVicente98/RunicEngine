// The GodotLayer (GodotLayer.hpp): the LibGodot instance inside the LayerStack.

#include "GodotLayer.hpp"

#include "Debug/DebugLayers.hpp"
#include "Debug/DevelopmentOverlay.hpp"
#include "Input/InputSink.hpp"
#include "Log.hpp"
#include "Presenter/Presenter.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <utility>

namespace Runic::GodotBridge {

struct GodotLayer::Impl {
    GodotLayerSpec spec;
    GodotRuntime runtime;
    Application::LayerContext *context = nullptr;
    Detail::RunicInputSink *sink = nullptr; // a node of Godot's scene tree, freed with it
    // Created once Godot runs (its RIDs need Godot's bindings), when the spec has frames.
    std::unique_ptr<Detail::DebugLayers> debug;
    std::unique_ptr<Detail::Presenter> presenter;
    std::array<bool, kDebugLayerCount> debugVisible{true, true, true}; // until debug exists
    bool headless = false;
};

GodotLayer::GodotLayer(GodotLayerSpec spec)
    : Layer(kGodotLayerName), m_Impl(std::make_unique<Impl>()) {
    Impl &impl = *m_Impl;
    impl.spec = std::move(spec);
    impl.headless = std::find(impl.spec.arguments.begin(), impl.spec.arguments.end(),
                              kHeadlessFlag) != impl.spec.arguments.end();
    if (impl.spec.forwardInput) {
        impl.runtime.AddInitializeHook([](GodotInitLevel level) {
            if (level == GodotInitLevel::Scene) {
                Detail::RegisterInputSink();
            }
        });
    }
    if (impl.spec.frames != nullptr) {
        if (impl.spec.developmentOverlay) {
            impl.runtime.AddInitializeHook([](GodotInitLevel level) {
                if (level == GodotInitLevel::Scene) {
                    Detail::RegisterDebugClasses();
                }
            });
        }
    }
}

GodotLayer::~GodotLayer() = default; // the runtime destroys a Godot instance that still exists

Core::Result<void> GodotLayer::OnAttach(Application::LayerContext &context) {
    Impl &impl = *m_Impl;
    impl.context = &context;
    const std::string library = impl.spec.libraryPath.empty()
                                    ? GodotRuntime::DefaultLibraryPath(impl.spec.target)
                                    : impl.spec.libraryPath;
    if (!impl.runtime.Load(library) || !impl.runtime.Create(impl.spec.arguments) ||
        !impl.runtime.Start()) {
        const std::string error = impl.runtime.LastError();
        impl.runtime.Destroy();
        return Core::Fail("Godot could not start: " + error);
    }
    if (impl.spec.forwardInput) {
        impl.sink = Detail::AttachInputSink(context);
    }
    if (impl.spec.frames != nullptr) {
        impl.debug = std::make_unique<Detail::DebugLayers>();
        for (std::size_t i = 0; i < impl.debugVisible.size(); ++i) {
            impl.debug->SetLayerVisible(static_cast<DebugLayer>(i), impl.debugVisible[i]);
        }
        impl.debug->Create(impl.spec.developmentOverlay);
        impl.presenter = std::make_unique<Detail::Presenter>();
        impl.presenter->Create();
    }
    RUNIC_LOG_INFO("RUNIC: Godot started {} ({} library)",
                   impl.headless ? "headless" : "with a window",
                   impl.spec.target == GodotTarget::Editor ? "editor" : "template");
    return {};
}

void GodotLayer::OnUpdate(const Application::FrameTime &) {
    Impl &impl = *m_Impl;
    if (impl.debug == nullptr || !impl.debug->IsCreated()) {
        return;
    }
    const RenderData::RenderFrame *frame = impl.spec.frames->ReadNewest();
    if (frame != nullptr && frame->number != impl.debug->Stats().frame) {
        // The presenter first: it decides where the frame's origin is in Godot's space.
        const Core::Vec3 shift = impl.presenter->Apply(*frame);
        impl.debug->Apply(*frame, shift);
    }
}

void GodotLayer::OnRender() {
    Impl &impl = *m_Impl;
    if (!impl.runtime.IsStarted()) {
        return;
    }
    const std::uint64_t frame = impl.runtime.FrameCount();
    for (const ScriptedClick &click : impl.spec.scriptedClicks) {
        const bool press = click.frame == frame;
        if (press || click.frame + kReleaseAfterFrames == frame) {
            Detail::PushClick(click.x, click.y, press);
        }
    }
    for (const ScriptedKey &key : impl.spec.scriptedKeys) {
        const bool press = key.frame == frame;
        if (press || key.frame + kReleaseAfterFrames == frame) {
            Detail::PushKey(key.key, press);
        }
    }
    if (impl.runtime.Iterate()) {
        // Godot quit: its window closed, or the frame limit (kQuitAfterFlag) was reached.
        impl.context->RequestStop(Application::kCleanExit);
    }
}

void GodotLayer::OnDetach() {
    Impl &impl = *m_Impl;
    impl.sink = nullptr; // freed with the scene tree
    if (impl.presenter != nullptr) {
        impl.presenter->Destroy(); // its instances go while Godot still runs
        impl.presenter.reset();
    }
    if (impl.debug != nullptr) {
        impl.debug->Destroy(); // its RenderingServer resources go while Godot still runs
        impl.debug.reset();
    }
    const std::uint64_t frames = impl.runtime.FrameCount();
    impl.runtime.Destroy();
    RUNIC_LOG_INFO("RUNIC: Godot stopped after {} frames", frames);
}

GodotRuntime &GodotLayer::Runtime() noexcept { return m_Impl->runtime; }
bool GodotLayer::IsHeadless() const noexcept { return m_Impl->headless; }
std::uint64_t GodotLayer::GodotFrames() const noexcept { return m_Impl->runtime.FrameCount(); }

void GodotLayer::SetDebugLayerVisible(DebugLayer layer, bool visible) {
    m_Impl->debugVisible[static_cast<std::size_t>(layer)] = visible;
    if (m_Impl->debug != nullptr) {
        m_Impl->debug->SetLayerVisible(layer, visible);
    }
}

bool GodotLayer::IsDebugLayerVisible(DebugLayer layer) const noexcept {
    // The debug layers hold the state once they exist: the overlay's checkboxes change it there.
    return m_Impl->debug != nullptr ? m_Impl->debug->IsLayerVisible(layer)
                                    : m_Impl->debugVisible[static_cast<std::size_t>(layer)];
}

DebugViewStats GodotLayer::DebugStats() const noexcept {
    return m_Impl->debug != nullptr ? m_Impl->debug->Stats() : DebugViewStats{};
}

PresenterStats GodotLayer::PresenterReport() const noexcept {
    return m_Impl->presenter != nullptr ? m_Impl->presenter->Stats() : PresenterStats{};
}

bool GodotLayer::ReadBackInstance(Core::RenderInstanceId id, PresentedInstance &out) const {
    return m_Impl->presenter != nullptr && m_Impl->presenter->ReadBack(id, out);
}

bool GodotLayer::ReadBackDebugSurface(RenderData::DebugBatch batch,
                                      RenderData::DebugPrimitive primitive,
                                      std::vector<Core::Vec3> &positions,
                                      std::vector<RenderData::PackedColor> &colors) const {
    positions.clear();
    colors.clear();
    return m_Impl->debug != nullptr && m_Impl->debug->ReadBack(batch, primitive, positions, colors);
}

} // namespace Runic::GodotBridge
