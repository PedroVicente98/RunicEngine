#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include "AppEvent.hpp"
#include "FrameTime.hpp"
#include "Result.hpp"

namespace Runic::Application {

class CommandLine;
class Layer;

// What a layer may ask of the host that runs it. It is handed to OnAttach and lives as long as the
// host, so a layer may keep it; there is no global application to reach instead (RunicDocs ›
// Application and LayerStack › Implementation rules).
class LayerContext {
  public:
    virtual ~LayerContext() = default;

    // The process's command line, every argument unchanged.
    virtual const CommandLine &Command() const = 0;
    // Frames completed so far.
    virtual std::uint64_t FramesRun() const = 0;
    // Queues an event, dispatched top-down at the start of the next frame. A full queue drops it
    // and counts the drop.
    virtual void PostEvent(const AppEvent &event) = 0;
    // The loop ends after the current frame's safe point. The first request's exit code wins.
    // Thread-safe.
    virtual void RequestStop(int exitCode) = 0;

    // Layer changes, applied in the order asked at the frame's safe point, never during a frame.
    virtual void PushLayer(std::unique_ptr<Layer> layer) = 0;
    virtual void PushOverlay(std::unique_ptr<Layer> overlay) = 0;
    virtual void PopLayer(const Layer &layer) = 0; // detached, then destroyed
    virtual void ReplaceLayer(const Layer &current,
                              std::unique_ptr<Layer> replacement) = 0; // current detaches first
};

// A coarse feature of an executable (RunicDocs › Application and LayerStack): it receives events
// top-down and is updated and rendered bottom-up. The LayerStack owns it; it never blocks (I/O runs
// on other threads and enqueues).
class Layer {
  public:
    explicit Layer(std::string name);
    virtual ~Layer();
    Layer(const Layer &) = delete;
    Layer &operator=(const Layer &) = delete;

    const std::string &Name() const noexcept;

    // Acquires resources. An error detaches nothing else by itself: the host stops the application
    // with exit code 1.
    virtual Core::Result<void> OnAttach(LayerContext &context);
    // Releases everything OnAttach acquired.
    virtual void OnDetach();
    // Top to bottom; set event.handled to stop the event here.
    virtual void OnEvent(AppEvent &event);
    // Bottom to top, once per frame.
    virtual void OnUpdate(const FrameTime &time);
    // Bottom to top, after every layer's OnUpdate (the client and the tools draw here).
    virtual void OnRender();

  private:
    std::string m_Name;
};

} // namespace Runic::Application
