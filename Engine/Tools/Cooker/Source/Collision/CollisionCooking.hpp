#pragma once

#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "CookInput.hpp"
#include "CookedFormats.hpp"
#include "Result.hpp"

// Collision cooking (COOK-1): an asset's collision triangles, scaled for a placement, welded and
// cleaned, then proven buildable by Jolt through Runic::Physics. Private to Runic::Cooker.

namespace Runic::Cooker::Detail {

struct CleanedMesh {
    Assets::CollisionMesh mesh;
    std::uint64_t trianglesIn = 0;
    std::uint64_t welded = 0;  // vertices merged into another
    std::uint64_t dropped = 0; // degenerate triangles
};

// The meshes' triangles as one mesh, each position scaled, then welded to the tolerance's grid,
// degenerate triangles dropped and unused vertices removed.
CleanedMesh CleanCollision(std::span<const InputMesh> meshes, Core::Vec3 scale,
                           float weldToleranceM);

// The visual mesh: the surfaces as one, normals kept or computed (area-weighted), bounds.
Assets::CookedMesh MergeVisual(std::span<const InputMesh> surfaces);

// Builds each collision mesh into a Jolt mesh shape (through Runic::Physics): an error says why
// Jolt refused one.
class JoltCheck {
  public:
    JoltCheck();
    ~JoltCheck();
    JoltCheck(const JoltCheck &) = delete;
    JoltCheck &operator=(const JoltCheck &) = delete;
    Core::Result<void> Check(std::string_view name, const Assets::CollisionMesh &mesh);

  private:
    struct State;
    std::unique_ptr<State> m_State;
};

} // namespace Runic::Cooker::Detail
