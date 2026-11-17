#pragma once

#include <functional>
#include <string>
#include <string_view>
#include <vector>

#include "FrameTime.hpp"
#include "InputMapping.hpp"
#include "Layer.hpp"

// The Input layer (CLI-2): the Application layer that owns the client's InputMap. It sits below
// the GodotLayer, so it receives only the input Godot's UI did not consume, and only what no layer
// above it handled. Each update it runs the command builders in the order they were added, then
// ends the frame of the map: a builder reads the actions and pushes the commands they mean into
// the simulation's command queues (Simulation::CommandQueue), the only door input has into the
// world. It handles no event: layers below still see them.

namespace Runic::Client {

inline constexpr std::string_view kInputLayerName = "Input";

// Reads the actions of this frame and pushes the commands they mean.
using CommandBuilder = std::function<void(const InputMap &map, const Application::FrameTime &time)>;

class InputLayer final : public Application::Layer {
  public:
    InputLayer();
    ~InputLayer() override;

    InputMap &Map() noexcept { return m_Map; }
    const InputMap &Map() const noexcept { return m_Map; }

    // A builder, run every update after the ones added before it. The name is for logs.
    void AddCommandBuilder(std::string_view name, CommandBuilder builder);

    // Keys and mouse buttons go to the map; losing the window's focus releases every action.
    void OnEvent(Application::AppEvent &event) override;
    // The builders, then the map's EndFrame.
    void OnUpdate(const Application::FrameTime &time) override;

  private:
    struct Builder {
        std::string name;
        CommandBuilder build;
    };

    InputMap m_Map;
    std::vector<Builder> m_Builders;
};

} // namespace Runic::Client
