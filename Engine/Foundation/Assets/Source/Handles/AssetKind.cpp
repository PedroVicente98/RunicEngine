// Asset kinds by name (AssetHandle.hpp).

#include "AssetHandle.hpp"

namespace Runic::Assets {

namespace {

constexpr std::string_view kMeshName = "mesh";
constexpr std::string_view kCollisionName = "collision";
constexpr std::string_view kCellName = "cell";

} // namespace

std::string_view AssetKindName(AssetKind kind) noexcept {
    switch (kind) {
    case AssetKind::Mesh:
        return kMeshName;
    case AssetKind::Collision:
        return kCollisionName;
    case AssetKind::Cell:
        return kCellName;
    }
    return kMeshName;
}

std::optional<AssetKind> ParseAssetKind(std::string_view text) noexcept {
    for (const AssetKind kind : {AssetKind::Mesh, AssetKind::Collision, AssetKind::Cell}) {
        if (AssetKindName(kind) == text) {
            return kind;
        }
    }
    return std::nullopt;
}

} // namespace Runic::Assets
