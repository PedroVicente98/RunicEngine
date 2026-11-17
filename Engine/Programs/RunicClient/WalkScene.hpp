#pragma once

#include <memory>
#include <string_view>

#include "ClientCamera.hpp"
#include "ExtractorRegistry.hpp"
#include "InputLayer.hpp"
#include "Layer.hpp"
#include "SimulationRuntime.hpp"

// RunicClient's walk scene (slice 7's runnable result): a capsule walks on static terrain 8,600 km
// from the world's origin under keyboard control. The terrain is a Jolt mesh far out; the capsule
// is a physics character (SIM-8, PHYS-2); keys become actions in the Input layer (CLI-2), a
// command builder turns the actions into walk commands, and the Commands phase hands them to the
// character. The camera follows the capsule. Neutral content: engine: names only.
//
//   W A S D     walk (relative to the camera)      Shift+W   run (its own action)
//   Space       jump                               R         walk on its own, on and off
//   left click  mark (logged; a click on the test button stays in Godot's UI)

namespace Runic::Programs {

inline constexpr std::string_view kWalkSceneLayerName = "engine:walk_scene";
inline constexpr std::string_view kTerrainExtractorName = "engine:demo_terrain";

class WalkSceneLayer final : public Application::Layer {
  public:
    // runtime: the local simulation (physics is enabled on it here, before anything else adds
    // physics systems); camera: the Presentation layer's; input: the Input layer, whose map gets
    // the scene's actions and preset and which runs its command builder. All outlive this layer.
    WalkSceneLayer(Simulation::SimulationRuntime &runtime, Client::ClientCamera &camera,
                   Client::InputLayer &input);
    ~WalkSceneLayer() override;

    // Registers the terrain extractor (the terrain's triangles and the controls above the
    // capsule) on the Presentation layer's registry.
    void RegisterExtractors(Client::ExtractorRegistry &registry);

    // Logs what the capsule did.
    void OnDetach() override;

  private:
    struct State;
    std::unique_ptr<State> m_State;
};

} // namespace Runic::Programs
