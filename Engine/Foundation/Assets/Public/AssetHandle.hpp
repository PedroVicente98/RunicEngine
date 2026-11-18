#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string_view>

#include "PlainIds.hpp"

// Asset handles (ASSET-1). An asset is cooked data a process loads by its Core::AssetId: a mesh, an
// asset's collision, a cell. A handle shares the loaded data: the cache keeps one copy however many
// handles name it, and frees it once no handle does and the cache collects. Loaded data never
// changes: a new cook is a new load.

namespace Runic::Assets {

enum class AssetKind : std::uint8_t {
    Mesh,      // CookedMesh: an asset's visual geometry in its own space
    Collision, // CookedCollision: an asset's collision shapes in its own space
    Cell,      // CookedCell: placements and colliders around a world position
};

inline constexpr std::uint32_t kAssetKindCount = 3;

// "mesh", "collision", "cell": how the catalog writes a kind.
std::string_view AssetKindName(AssetKind kind) noexcept;
std::optional<AssetKind> ParseAssetKind(std::string_view text) noexcept;

template <class T> class AssetHandle {
  public:
    AssetHandle() = default;
    AssetHandle(Core::AssetId id, std::shared_ptr<const T> data)
        : m_Id(id), m_Data(std::move(data)) {}

    Core::AssetId Id() const noexcept { return m_Id; }
    bool IsLoaded() const noexcept { return m_Data != nullptr; }
    const T &Get() const noexcept { return *m_Data; }
    const T *operator->() const noexcept { return m_Data.get(); }
    const T &operator*() const noexcept { return *m_Data; }
    // Handles of one load share it: the same address.
    friend bool operator==(const AssetHandle &a, const AssetHandle &b) noexcept {
        return a.m_Data == b.m_Data && a.m_Id == b.m_Id;
    }

  private:
    Core::AssetId m_Id;
    std::shared_ptr<const T> m_Data;
};

} // namespace Runic::Assets
