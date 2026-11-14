#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include "Layer.hpp"
#include "LayerStack.hpp"
#include "LoopPolicy.hpp"

namespace Runic::Application {

struct ApplicationSpec {
    std::string name;                        // for logs, such as "RunicServer"
    LoopPolicy loop = LoopPolicy::Fixed(60); // how frames are paced
    std::uint64_t maxFrames = 0; // stop after this many frames; 0 runs until a stop request
};

// One executable's composition: its spec and its layers. CreateApplication builds it and the
// ApplicationHost runs it. A program may derive from it to hear when it stops.
class Application {
  public:
    explicit Application(ApplicationSpec spec);
    virtual ~Application();
    Application(const Application &) = delete;
    Application &operator=(const Application &) = delete;

    const ApplicationSpec &Spec() const noexcept;
    // Attached in this order before the first frame.
    void PushLayer(std::unique_ptr<Layer> layer);
    void PushOverlay(std::unique_ptr<Layer> overlay);
    LayerStack &Layers() noexcept;

    // After every layer detached: the exit code and the frames run. A program prints its closing
    // line here.
    virtual void OnStopped(int exitCode, std::uint64_t frames);

  private:
    ApplicationSpec m_Spec;
    LayerStack m_Layers;
};

} // namespace Runic::Application
