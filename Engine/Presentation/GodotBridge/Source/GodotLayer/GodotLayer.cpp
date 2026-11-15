// The GodotLayer (GodotLayer.hpp): the LibGodot instance inside the LayerStack.

#include "GodotLayer.hpp"

#include "Input/InputSink.hpp"
#include "Log.hpp"

#include <algorithm>
#include <utility>

namespace Runic::GodotBridge {

struct GodotLayer::Impl {
    GodotLayerSpec spec;
    GodotRuntime runtime;
    Application::LayerContext *context = nullptr;
    Detail::RunicInputSink *sink = nullptr; // a node of Godot's scene tree, freed with it
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
    RUNIC_LOG_INFO("RUNIC: Godot started {} ({} library)",
                   impl.headless ? "headless" : "with a window",
                   impl.spec.target == GodotTarget::Editor ? "editor" : "template");
    return {};
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
    const std::uint64_t frames = impl.runtime.FrameCount();
    impl.runtime.Destroy();
    RUNIC_LOG_INFO("RUNIC: Godot stopped after {} frames", frames);
}

GodotRuntime &GodotLayer::Runtime() noexcept { return m_Impl->runtime; }
bool GodotLayer::IsHeadless() const noexcept { return m_Impl->headless; }
std::uint64_t GodotLayer::GodotFrames() const noexcept { return m_Impl->runtime.FrameCount(); }

} // namespace Runic::GodotBridge
