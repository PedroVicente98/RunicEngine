#pragma once

#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "AssetHandle.hpp"
#include "ContentId.hpp"
#include "ContentRegistry.hpp"
#include "PlainIds.hpp"
#include "Result.hpp"

// The asset catalog (ASSET-1): what a cooked folder holds. Each asset has its AssetId (from
// Godot's stable UID, mapped by the Cooker), its kind, its content name (game:rocks/test_region)
// and its file, relative to the folder. The Cooker writes the catalog as text, one asset a line,
// so a person can read it and a diff shows what a cook changed:
//
//     runic-assets 1
//     mesh       00a1b2c3d4e5f601  game:test_region           Meshes/00a1b2c3d4e5f601.rmesh
//
// (the columns separated by tabs). Reading it reports every problem with its line: a duplicate
// name or ID, a malformed name, an unknown kind, a line with too few columns.

namespace Runic::Assets {

inline constexpr std::string_view kCatalogFileName = "catalog.txt";
inline constexpr std::string_view kCatalogHeader = "runic-assets 1";
inline constexpr char kCatalogColumnSeparator = '\t';
inline constexpr char kCatalogComment = '#';

struct AssetEntry {
    Core::AssetId id;
    AssetKind kind = AssetKind::Mesh;
    Content::ContentId name;
    std::string file; // relative to the cooked folder, '/' between folders
};

class AssetCatalog {
  public:
    AssetCatalog() = default;
    AssetCatalog(const AssetCatalog &) = delete;
    AssetCatalog &operator=(const AssetCatalog &) = delete;
    AssetCatalog(AssetCatalog &&) noexcept = default;
    AssetCatalog &operator=(AssetCatalog &&) noexcept = default;

    // Adds an asset; a duplicate name or ID is a problem, and the first stays.
    bool Add(AssetEntry entry, const Content::ContentSource &source);

    const AssetEntry *Find(Core::AssetId id) const noexcept;
    const AssetEntry *Find(std::string_view name) const noexcept;
    std::size_t Count() const noexcept { return m_Names.Count(); }
    // Every asset in the order added.
    std::vector<AssetEntry> Entries() const;

    // The problems found adding and reading, in the order found.
    std::span<const Content::ContentProblem> Problems() const noexcept { return m_Problems; }
    bool HasProblems() const noexcept { return !m_Problems.empty(); }

    // The catalog's text, and back (problems collected, with lines in file).
    std::string Write() const;
    static AssetCatalog Read(std::string_view text, std::string_view file);

  private:
    void Report(Content::ContentProblem problem);

    Content::ContentRegistry<AssetEntry> m_Names{"asset"};
    std::unordered_map<Core::AssetId, Content::ContentId> m_Ids;
    std::vector<Content::ContentProblem> m_Problems;
};

// An AssetId as the catalog and file names write it: 16 lowercase hex digits.
std::string AssetIdText(Core::AssetId id);
Core::Result<Core::AssetId> ParseAssetId(std::string_view text);

} // namespace Runic::Assets
