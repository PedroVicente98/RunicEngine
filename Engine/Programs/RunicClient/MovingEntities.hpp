#pragma once

#include <memory>
#include <string_view>

#include "ClientCamera.hpp"
#include "FrameTime.hpp"
#include "Layer.hpp"
#include "SimulationRuntime.hpp"

// RunicClient's moving entities (slice 5's runnable result): walkers patrol squares on a plane
// around the slice 2 scene, steered only through a command queue. A driver at 20 Hz decides where
// each walker heads and queues a Steer command; the Commands phase applies it; the Movement phase
// moves the walker; the Presentation layer draws it as a debug shape the presenter keeps in Godot.
// The layer also flies the client camera around the plane. Neutral content: engine: names only.

namespace Runic::Programs {

inline constexpr std::string_view kMovingEntitiesLayerName = "engine:moving_entities";

class MovingEntitiesLayer final : public Application::Layer {
  public:
    // runtime: the local simulation, owned by the Simulation layer below this one; camera: the
    // Presentation layer's, above. Both outlive this layer.
    MovingEntitiesLayer(Simulation::SimulationRuntime &runtime, Client::ClientCamera &camera);
    ~MovingEntitiesLayer() override;

    // Moves the camera along its orbit.
    void OnUpdate(const Application::FrameTime &time) override;
    // Logs what the walkers did.
    void OnDetach() override;

  private:
    struct State;
    std::unique_ptr<State> m_State;
};

} // namespace Runic::Programs
