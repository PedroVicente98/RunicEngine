// The asset cache (AssetCache.hpp).

#include "AssetCache.hpp"

#include <format>
#include <string>

namespace Runic::Assets {

namespace {

constexpr std::size_t kProblemsShown = 10;

// The bytes an asset holds in memory, for the stats.
std::uint64_t BytesOf(const CookedMesh &mesh) {
    return (mesh.positions.size() + mesh.normals.size()) * sizeof(Core::Vec3) +
           mesh.indices.size() * sizeof(std::uint32_t);
}
std::uint64_t BytesOf(const CookedCollision &collision) {
    std::uint64_t bytes = collision.shapes.size() * sizeof(CollisionShape);
    for (const CollisionMesh &mesh : collision.meshes) {
        bytes += mesh.positions.size() * sizeof(Core::Vec3) +
                 mesh.indices.size() * sizeof(std::uint32_t);
    }
    return bytes;
}
std::uint64_t BytesOf(const CookedCell &cell) {
    return cell.placements.size() * sizeof(CellPlacement) +
           cell.colliders.size() * sizeof(CellCollider);
}

Core::Result<CookedMesh> Decode(std::span<const std::byte> bytes, const CookedMesh *) {
    return ReadCookedMesh(bytes);
}
Core::Result<CookedCollision> Decode(std::span<const std::byte> bytes, const CookedCollision *) {
    return ReadCookedCollision(bytes);
}
Core::Result<CookedCell> Decode(std::span<const std::byte> bytes, const CookedCell *) {
    return ReadCookedCell(bytes);
}

std::string_view Article(AssetKind kind) noexcept {
    return kind == AssetKind::Mesh        ? "a mesh"
           : kind == AssetKind::Collision ? "a collision"
                                          : "a cell";
}

} // namespace

AssetCache::AssetCache(std::filesystem::path root) : m_Root(std::move(root)) {}

Core::Result<void> AssetCache::Open() {
    const std::filesystem::path path = m_Root / kCatalogFileName;
    auto bytes = ReadBytesFromFile(path);
    if (!bytes) {
        return Core::Fail(
            std::format("the cooked folder has no catalog: {}", bytes.GetError().message));
    }
    const std::string text(reinterpret_cast<const char *>(bytes.Value().data()),
                           bytes.Value().size());
    AssetCatalog catalog = AssetCatalog::Read(text, path.generic_string());
    const std::lock_guard lock(m_Mutex);
    m_Catalog = std::move(catalog);
    if (!m_Catalog.HasProblems()) {
        return {};
    }
    std::string message =
        std::format("{} has {} problems:", path.generic_string(), m_Catalog.Problems().size());
    std::size_t shown = 0;
    for (const Content::ContentProblem &problem : m_Catalog.Problems()) {
        if (shown++ == kProblemsShown) {
            break;
        }
        message += "\n  " + problem.message;
    }
    return Core::Fail(std::move(message));
}

template <class T> Core::Result<AssetHandle<T>> AssetCache::Load(Core::AssetId id, AssetKind kind) {
    const std::lock_guard lock(m_Mutex);
    if (const auto held = m_Held.find(id); held != m_Held.end() && held->second.kind == kind) {
        ++m_Stats.hits;
        return AssetHandle<T>(id, std::static_pointer_cast<const T>(held->second.data));
    }
    const AssetEntry *entry = m_Catalog.Find(id);
    if (entry == nullptr) {
        ++m_Stats.failures;
        return Core::Fail(std::format("asset {} is not in the catalog", AssetIdText(id)));
    }
    if (entry->kind != kind) {
        ++m_Stats.failures;
        return Core::Fail(std::format("asset {} ({}) is {}, not {}", AssetIdText(id),
                                      entry->name.Text(), Article(entry->kind), Article(kind)));
    }
    const auto failed = [&](const std::string &why) {
        ++m_Stats.failures;
        return Core::Fail(std::format("asset {} ({}, {}): {}", AssetIdText(id), entry->name.Text(),
                                      entry->file, why));
    };
    auto bytes = ReadBytesFromFile(m_Root / entry->file);
    if (!bytes) {
        return failed(bytes.GetError().message);
    }
    auto decoded = Decode(bytes.Value(), static_cast<const T *>(nullptr));
    if (!decoded) {
        return failed(decoded.GetError().message);
    }
    auto data = std::make_shared<const T>(std::move(decoded.Value()));
    ++m_Stats.loads;
    m_Held[id] = Held{kind, data, BytesOf(*data)};
    return AssetHandle<T>(id, std::move(data));
}

Core::Result<Core::AssetId> AssetCache::IdOf(std::string_view name) const {
    const std::lock_guard lock(m_Mutex);
    const AssetEntry *entry = m_Catalog.Find(name);
    if (entry == nullptr) {
        return Core::Fail(std::format("no asset is named {} in the catalog", name));
    }
    return entry->id;
}

Core::Result<AssetHandle<CookedMesh>> AssetCache::LoadMesh(Core::AssetId id) {
    return Load<CookedMesh>(id, AssetKind::Mesh);
}
Core::Result<AssetHandle<CookedCollision>> AssetCache::LoadCollision(Core::AssetId id) {
    return Load<CookedCollision>(id, AssetKind::Collision);
}
Core::Result<AssetHandle<CookedCell>> AssetCache::LoadCell(Core::AssetId id) {
    return Load<CookedCell>(id, AssetKind::Cell);
}

template <class Loaded, class LoadById>
Core::Result<Loaded> ByName(Core::Result<Core::AssetId> id, std::uint64_t &failures,
                            std::mutex &mutex, LoadById &&load) {
    if (!id) {
        const std::lock_guard lock(mutex);
        ++failures;
        return id.GetError();
    }
    return load(id.Value());
}

Core::Result<AssetHandle<CookedMesh>> AssetCache::LoadMesh(std::string_view name) {
    return ByName<AssetHandle<CookedMesh>>(IdOf(name), m_Stats.failures, m_Mutex,
                                           [this](Core::AssetId id) { return LoadMesh(id); });
}
Core::Result<AssetHandle<CookedCollision>> AssetCache::LoadCollision(std::string_view name) {
    return ByName<AssetHandle<CookedCollision>>(
        IdOf(name), m_Stats.failures, m_Mutex,
        [this](Core::AssetId id) { return LoadCollision(id); });
}
Core::Result<AssetHandle<CookedCell>> AssetCache::LoadCell(std::string_view name) {
    return ByName<AssetHandle<CookedCell>>(IdOf(name), m_Stats.failures, m_Mutex,
                                           [this](Core::AssetId id) { return LoadCell(id); });
}

std::size_t AssetCache::Collect() {
    const std::lock_guard lock(m_Mutex);
    std::size_t freed = 0;
    for (auto held = m_Held.begin(); held != m_Held.end();) {
        if (held->second.data.use_count() == 1) { // only the cache holds it
            held = m_Held.erase(held);
            ++freed;
        } else {
            ++held;
        }
    }
    return freed;
}

AssetCacheStats AssetCache::Stats() const {
    const std::lock_guard lock(m_Mutex);
    AssetCacheStats stats = m_Stats;
    stats.resident = static_cast<std::uint32_t>(m_Held.size());
    stats.residentBytes = 0;
    for (const auto &[id, held] : m_Held) {
        stats.residentBytes += held.bytes;
    }
    return stats;
}

} // namespace Runic::Assets
