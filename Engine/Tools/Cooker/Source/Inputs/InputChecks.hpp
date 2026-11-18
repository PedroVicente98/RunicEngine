#pragma once

#include <string>
#include <vector>

#include "CookInput.hpp"

// What the Cooker checks in what it reads before it cooks anything (COOK-1): every problem, each
// a sentence naming the scene or the asset's source, and the node or the asset. Private to
// Runic::Cooker.

namespace Runic::Cooker::Detail {

// An asset's own data: its geometry, its collision preset.
void CheckAsset(const InputAsset &asset, std::vector<std::string> &problems);
// A scene's placements and colliders, against the assets it was handed.
void CheckScene(const CookScene &scene, std::vector<std::string> &problems);

} // namespace Runic::Cooker::Detail
