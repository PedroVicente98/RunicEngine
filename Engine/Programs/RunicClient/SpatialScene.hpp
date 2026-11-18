#pragma once

#include <memory>
#include <string_view>

#include "ClientCamera.hpp"
#include "ExtractorRegistry.hpp"
#include "FrameTime.hpp"
#include "Layer.hpp"
#include "SimulationRuntime.hpp"

// RunicClient's spatial scene (slice 11's runnable result, --spatial-scene): the sparse 3D
// spatial index and authored volumes in the debug view, 6,400 km from the world's origin.
//
// 150 walkers drift and bounce in a 200 m box. They also climb and dive, so their cells reach a
// cave below the ground and an island in the sky. The PostPhysics phase updates their membership,
// and the Queries phase runs one query a tick, a sphere, a box or an ellipsoid that sweeps from
// the cave up to the island. Each query is checked against testing every walker.
//
// The view draws:
//   - the occupied cells (dim gray boxes);
//   - the walkers (small orange boxes);
//   - the query in magenta, with what it found;
//   - the volumes: the cave and a quest area (rule areas, orange), the island (also with its
//     bounds in white: it is indivisible) and a ship's deck circling the scene (a sensor, cyan).
//
// The camera circles the scene. Neutral content: engine: names only.

namespace Runic::Programs {

inline constexpr std::string_view kSpatialSceneLayerName = "engine:spatial_scene";
inline constexpr std::string_view kSpatialViewExtractorName = "engine:demo_spatial_view";

class SpatialSceneLayer final : public Application::Layer {
  public:
    // runtime: the local simulation, owned by the Simulation layer below this one; camera: the
    // Presentation layer's, above. Both outlive this layer.
    SpatialSceneLayer(Simulation::SimulationRuntime &runtime, Client::ClientCamera &camera);
    ~SpatialSceneLayer() override;

    // Registers the extractor that draws the index, the query and the volumes.
    void RegisterExtractors(Client::ExtractorRegistry &registry);

    // Moves the camera along its orbit.
    void OnUpdate(const Application::FrameTime &time) override;
    // Logs what the index held and whether every query matched testing every walker.
    void OnDetach() override;

  private:
    struct State;
    std::unique_ptr<State> m_State;
};

} // namespace Runic::Programs
