#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "ContentId.hpp"
#include "Result.hpp"

// Registries and lookup (CNT-1). A registry holds the definitions of one kind (items, abilities,
// assets) by their ContentId, and remembers where each came from. Loading never stops at the first
// mistake: a duplicate keeps the first definition and is reported with both sources, an ID
// referenced but defined nowhere is reported with where it was referenced, a malformed one with
// where it was written, and the caller reads every problem at once ("Duplicate and unknown IDs are
// reported with their source"). A registry is filled and read on one thread (the loader's);
// definitions keep their address while it lives.

namespace Runic::Content {

// Where a definition or a reference came from: a project file and its line, or what made it when no
// file did ("engine code", a test).
struct ContentSource {
    std::string file;       // project-relative, '/' between folders
    std::uint32_t line = 0; // 0: the file has no lines, or the line is not known
    std::string origin;     // what made it, when there is no file

    // "Data/items.json:12", "Data/items.json", "engine code", or "an unknown source".
    std::string ToText() const;
    friend bool operator==(const ContentSource &, const ContentSource &) = default;
};

inline constexpr std::uint32_t kNoLine = 0;

ContentSource FromFile(std::string_view file, std::uint32_t line = kNoLine);
ContentSource FromCode(std::string_view origin);

enum class ContentProblemKind : std::uint8_t {
    Duplicate, // defined twice: source is the second definition, firstSource the one kept
    Unknown,   // referenced, defined nowhere: source is the reference
    Malformed, // not a content ID: source is where it was written
};

struct ContentProblem {
    ContentProblemKind kind = ContentProblemKind::Unknown;
    std::string id; // as written
    ContentSource source;
    ContentSource firstSource; // Duplicate only
    // One sentence naming the registry's kind, the ID and the sources, for a log or a cook report.
    std::string message;
};

namespace Detail {

// What every registry shares: its kind and its problems.
class RegistryBase {
  public:
    explicit RegistryBase(std::string_view kind) : m_Kind(kind) {}

    // What the registry holds, in the singular: "item", "asset".
    std::string_view Kind() const noexcept { return m_Kind; }
    std::span<const ContentProblem> Problems() const noexcept { return m_Problems; }
    bool HasProblems() const noexcept { return !m_Problems.empty(); }
    void ClearProblems() noexcept { m_Problems.clear(); }

  protected:
    void ReportDuplicate(const ContentId &id, const ContentSource &second,
                         const ContentSource &first);
    void ReportUnknown(std::string_view id, const ContentSource &from);
    void ReportMalformed(std::string_view text, const ContentSource &from, const Core::Error &why);

  private:
    std::string m_Kind;
    std::vector<ContentProblem> m_Problems;
};

} // namespace Detail

template <class T> class ContentRegistry : public Detail::RegistryBase {
  public:
    explicit ContentRegistry(std::string_view kind) : RegistryBase(kind) {}

    // Adds a definition. A duplicate is reported with both sources, and the first stays: false.
    bool Add(const ContentId &id, T value, ContentSource source) {
        if (const Entry *first = FindEntry(id.Text(), id.Hash())) {
            ReportDuplicate(id, source, first->source);
            return false;
        }
        const auto index = static_cast<std::uint32_t>(m_Entries.size());
        m_Entries.push_back(Entry{id, std::move(value), std::move(source)});
        m_Index.emplace(id.Hash(), index);
        return true;
    }
    // Adds a definition under an ID as written; a malformed one is reported with its source.
    bool Add(std::string_view text, T value, ContentSource source) {
        auto id = ContentId::Parse(text);
        if (!id) {
            ReportMalformed(text, source, id.GetError());
            return false;
        }
        return Add(id.Value(), std::move(value), std::move(source));
    }

    // Lookups that report nothing: nullptr when the ID is unknown (or, as text, malformed).
    const T *Find(const ContentId &id) const noexcept {
        const Entry *entry = FindEntry(id.Text(), id.Hash());
        return entry != nullptr ? &entry->value : nullptr;
    }
    const T *Find(std::string_view text) const noexcept {
        const Entry *entry = FindEntry(text, HashText(text));
        return entry != nullptr ? &entry->value : nullptr;
    }
    bool Contains(const ContentId &id) const noexcept { return Find(id) != nullptr; }
    const ContentSource *SourceOf(const ContentId &id) const noexcept {
        const Entry *entry = FindEntry(id.Text(), id.Hash());
        return entry != nullptr ? &entry->source : nullptr;
    }

    // A reference from a definition or a file: what it names, or nullptr with the unknown (or
    // malformed) ID reported with where it was referenced.
    const T *Resolve(const ContentId &id, const ContentSource &from) {
        const T *found = Find(id);
        if (found == nullptr) {
            ReportUnknown(id.Text(), from);
        }
        return found;
    }
    const T *Resolve(std::string_view text, const ContentSource &from) {
        auto id = ContentId::Parse(text);
        if (!id) {
            ReportMalformed(text, from, id.GetError());
            return nullptr;
        }
        return Resolve(id.Value(), from);
    }

    std::size_t Count() const noexcept { return m_Entries.size(); }
    // Every definition in the order it was added: visit(const ContentId &, const T &, const
    // ContentSource &).
    template <class Visit> void ForEach(Visit &&visit) const {
        for (const Entry &entry : m_Entries) {
            visit(entry.id, entry.value, entry.source);
        }
    }

  private:
    struct Entry {
        ContentId id;
        T value;
        ContentSource source;
    };

    const Entry *FindEntry(std::string_view text, std::uint64_t hash) const noexcept {
        const auto [first, last] = m_Index.equal_range(hash);
        for (auto it = first; it != last; ++it) {
            const Entry &entry = m_Entries[it->second];
            if (entry.id.Text() == text) {
                return &entry;
            }
        }
        return nullptr;
    }

    std::deque<Entry> m_Entries; // a deque: definitions keep their address
    std::unordered_multimap<std::uint64_t, std::uint32_t> m_Index;
};

} // namespace Runic::Content
