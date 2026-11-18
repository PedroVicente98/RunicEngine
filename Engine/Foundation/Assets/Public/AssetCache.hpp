#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <map>
#include <memory>
#include <mutex>
#include <string_view>

#include "AssetCatalog.hpp"
#include "AssetHandle.hpp"
#include "CookedFormats.hpp"
#include "PlainIds.hpp"
#include "Result.hpp"

// The asset cache shared by client and server (ASSET-1). It opens a cooked folder by its catalog
// and loads an asset the first time it is asked for, by ID or by name; later loads share that copy.
// Collect frees what no handle holds. Any thread may load (the server's I/O threads, the client's
// loader): one lock guards the cache, and a load reads its file under it, so a simulation tick
// never loads (CLAUDE.md: never block a tick); streaming without the lock is ASSET-2's.

namespace Runic::Assets {

struct AssetCacheStats {
    std::uint64_t loads = 0;    // files read
    std::uint64_t hits = 0;     // loads answered by a copy already held
    std::uint64_t failures = 0; // loads that failed (unknown, missing, damaged, another kind)
    std::uint32_t resident = 0; // assets held now
    std::uint64_t residentBytes = 0;
};

class AssetCache {
  public:
    // root: a cooked folder, its catalog at root/catalog.txt.
    explicit AssetCache(std::filesystem::path root);

    // Reads the catalog; an error lists its problems (and the cache holds what read cleanly).
    Core::Result<void> Open();
    const AssetCatalog &Catalog() const noexcept { return m_Catalog; }
    const std::filesystem::path &Root() const noexcept { return m_Root; }

    // The asset, loaded once: an error names it, its file and what went wrong (not in the
    // catalog, another kind, missing, damaged).
    Core::Result<AssetHandle<CookedMesh>> LoadMesh(Core::AssetId id);
    Core::Result<AssetHandle<CookedCollision>> LoadCollision(Core::AssetId id);
    Core::Result<AssetHandle<CookedCell>> LoadCell(Core::AssetId id);
    // By the catalog's name.
    Core::Result<AssetHandle<CookedMesh>> LoadMesh(std::string_view name);
    Core::Result<AssetHandle<CookedCollision>> LoadCollision(std::string_view name);
    Core::Result<AssetHandle<CookedCell>> LoadCell(std::string_view name);

    // Frees what no handle holds; returns how many assets went.
    std::size_t Collect();
    AssetCacheStats Stats() const;

  private:
    struct Held {
        AssetKind kind = AssetKind::Mesh;
        std::shared_ptr<const void> data;
        std::uint64_t bytes = 0;
    };

    template <class T> Core::Result<AssetHandle<T>> Load(Core::AssetId id, AssetKind kind);
    Core::Result<Core::AssetId> IdOf(std::string_view name) const;

    std::filesystem::path m_Root;
    AssetCatalog m_Catalog;
    mutable std::mutex m_Mutex;
    std::map<Core::AssetId, Held> m_Held;
    AssetCacheStats m_Stats;
};

} // namespace Runic::Assets
