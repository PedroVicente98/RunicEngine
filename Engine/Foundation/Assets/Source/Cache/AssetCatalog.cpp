// The asset catalog (AssetCatalog.hpp).

#include "AssetCatalog.hpp"

#include <charconv>
#include <format>

namespace Runic::Assets {

namespace {

constexpr int kHexBase = 16;
constexpr std::size_t kAssetIdDigits = 16;
constexpr std::size_t kCatalogColumns = 4;
constexpr std::uint32_t kFirstLine = 1;
constexpr char kNewline = '\n';
constexpr char kCarriageReturn = '\r';

std::vector<std::string_view> Columns(std::string_view line) {
    std::vector<std::string_view> columns;
    std::size_t start = 0;
    while (true) {
        const std::size_t end = line.find(kCatalogColumnSeparator, start);
        columns.push_back(line.substr(start, end == std::string_view::npos ? end : end - start));
        if (end == std::string_view::npos) {
            return columns;
        }
        start = end + 1;
    }
}

Content::ContentProblem LineProblem(const Content::ContentSource &source, std::string id,
                                    std::string message) {
    Content::ContentProblem problem;
    problem.kind = Content::ContentProblemKind::Malformed;
    problem.id = std::move(id);
    problem.source = source;
    problem.message = std::move(message);
    return problem;
}

} // namespace

std::string AssetIdText(Core::AssetId id) { return std::format("{:016x}", id.Value()); }

Core::Result<Core::AssetId> ParseAssetId(std::string_view text) {
    std::uint64_t value = 0;
    const auto [end, error] =
        std::from_chars(text.data(), text.data() + text.size(), value, kHexBase);
    if (text.size() != kAssetIdDigits || error != std::errc() || end != text.data() + text.size() ||
        value == Core::AssetId::kInvalidValue) {
        return Core::Fail(std::format("'{}' is not an asset ID (16 hex digits, not all 0)", text));
    }
    return Core::AssetId(value);
}

void AssetCatalog::Report(Content::ContentProblem problem) {
    m_Problems.push_back(std::move(problem));
}

bool AssetCatalog::Add(AssetEntry entry, const Content::ContentSource &source) {
    if (const auto known = m_Ids.find(entry.id); known != m_Ids.end()) {
        Content::ContentProblem problem;
        problem.kind = Content::ContentProblemKind::Duplicate;
        problem.id = AssetIdText(entry.id);
        problem.source = source;
        if (const Content::ContentSource *first = m_Names.SourceOf(known->second)) {
            problem.firstSource = *first;
        }
        problem.message =
            std::format("asset ID {} is given twice: to {} at {}, and first to {} at {} (kept)",
                        AssetIdText(entry.id), entry.name.Text(), source.ToText(),
                        known->second.Text(), problem.firstSource.ToText());
        Report(std::move(problem));
        return false;
    }
    const Core::AssetId id = entry.id;
    const Content::ContentId name = entry.name;
    const std::size_t before = m_Names.Problems().size();
    if (!m_Names.Add(name, std::move(entry), source)) {
        for (std::size_t i = before; i < m_Names.Problems().size(); ++i) {
            Report(m_Names.Problems()[i]);
        }
        return false;
    }
    m_Ids.emplace(id, name);
    return true;
}

const AssetEntry *AssetCatalog::Find(Core::AssetId id) const noexcept {
    const auto known = m_Ids.find(id);
    return known != m_Ids.end() ? m_Names.Find(known->second) : nullptr;
}

const AssetEntry *AssetCatalog::Find(std::string_view name) const noexcept {
    return m_Names.Find(name);
}

std::vector<AssetEntry> AssetCatalog::Entries() const {
    std::vector<AssetEntry> entries;
    m_Names.ForEach([&](const Content::ContentId &, const AssetEntry &entry,
                        const Content::ContentSource &) { entries.push_back(entry); });
    return entries;
}

std::string AssetCatalog::Write() const {
    std::string text(kCatalogHeader);
    text += kNewline;
    m_Names.ForEach([&](const Content::ContentId &, const AssetEntry &entry,
                        const Content::ContentSource &) {
        text += std::format("{}{}{}{}{}{}{}{}", AssetKindName(entry.kind), kCatalogColumnSeparator,
                            AssetIdText(entry.id), kCatalogColumnSeparator, entry.name.Text(),
                            kCatalogColumnSeparator, entry.file, kNewline);
    });
    return text;
}

AssetCatalog AssetCatalog::Read(std::string_view text, std::string_view file) {
    AssetCatalog catalog;
    std::uint32_t number = kFirstLine;
    std::size_t start = 0;
    bool headerSeen = false;
    while (start < text.size()) {
        std::size_t end = text.find(kNewline, start);
        std::string_view line =
            text.substr(start, end == std::string_view::npos ? end : end - start);
        start = end == std::string_view::npos ? text.size() : end + 1;
        const Content::ContentSource source = Content::FromFile(file, number++);
        if (!line.empty() && line.back() == kCarriageReturn) {
            line.remove_suffix(1);
        }
        if (!headerSeen) {
            headerSeen = true;
            if (line != kCatalogHeader) {
                catalog.Report(LineProblem(source, std::string(line),
                                           std::format("{} starts with '{}', not '{}'",
                                                       source.ToText(), line, kCatalogHeader)));
                return catalog;
            }
            continue;
        }
        if (line.empty() || line.front() == kCatalogComment) {
            continue;
        }
        const std::vector<std::string_view> columns = Columns(line);
        if (columns.size() != kCatalogColumns) {
            catalog.Report(LineProblem(
                source, std::string(line),
                std::format("{} has {} columns: an asset is its kind, ID, name and file, "
                            "separated by tabs",
                            source.ToText(), columns.size())));
            continue;
        }
        const auto kind = ParseAssetKind(columns[0]);
        if (!kind.has_value()) {
            catalog.Report(
                LineProblem(source, std::string(columns[2]),
                            std::format("{} names the asset kind '{}' (mesh, collision or cell)",
                                        source.ToText(), columns[0])));
            continue;
        }
        const auto id = ParseAssetId(columns[1]);
        if (!id) {
            catalog.Report(
                LineProblem(source, std::string(columns[1]),
                            std::format("{}: {}", source.ToText(), id.GetError().message)));
            continue;
        }
        const auto name = Content::ContentId::Parse(columns[2]);
        if (!name) {
            catalog.Report(LineProblem(
                source, std::string(columns[2]),
                std::format("{} names the asset by {}", source.ToText(), name.GetError().message)));
            continue;
        }
        catalog.Add({id.Value(), *kind, name.Value(), std::string(columns[3])}, source);
    }
    if (!headerSeen) {
        catalog.Report(LineProblem(
            Content::FromFile(file), {},
            std::format("{} is empty: it should start with '{}'", file, kCatalogHeader)));
    }
    return catalog;
}

} // namespace Runic::Assets
