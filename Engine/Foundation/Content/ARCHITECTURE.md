# Runic::Content — module contract

Group: Foundation · Headless: **yes** · Target: `Runic::Content` (`runic_content`) · Packages: CNT-1, CNT-2

Source: RunicDocs › Architecture › Repositories, build targets and processes (Engine modules: "Namespaced ContentId
(engine:, game:), registries, versioned schemas, validation, loading, live reload"); Modules, work packages and the
implementation contract (CNT-1, CNT-2); World authoring and cooking (stable IDs, Data/). Status: **CNT-1 implemented**
(slice 9, on the preview branch `preview/slices-3-11`): content IDs, sources, registries and lookup, with their
acceptance tests. The folder map is [README.md](README.md); slice 9's brief and walkthrough are
[preview/slice-09-first-cooker-proof.md](../../../preview/slice-09-first-cooker-proof.md).

## Purpose

Everything data names has a content ID: an item, an ability, an asset, a component, a system, a queue. The namespace
says who defined it, the Engine (`engine:`) or the game built on it (`game:`), so a public repository never collides
with a game and a game never shadows the Engine. Registries hold the definitions of one kind by their ID and remember
where each came from, so loading reports every duplicate, unknown and malformed ID with its file and line at once,
instead of stopping at the first or resolving the wrong one silently.

## Public API

Namespace `Runic::Content`. Headers are included by their path under `Public/`. No library types.

| Header | Holds | Since |
| --- | --- | --- |
| `ContentId.hpp` | `ContentId` (`Parse`, `Of`, `IsValid`, `Namespace`, `Name`, `Text`, `Hash`; equality, ordering by text, `std::hash`), `ContentNamespace` (`Engine`, `Game`), `NamespaceText`, `HashText` (64-bit FNV-1a); `kEngineNamespace`, `kGameNamespace`, `kNamespaceSeparator`, `kNamePartSeparator`, `kMaxContentIdLength` (128) | CNT-1 |
| `ContentRegistry.hpp` | `ContentRegistry<T>` (`Add` by ID or text, `Find` by ID or text, `Contains`, `SourceOf`, `Resolve` by ID or text, `Count`, `ForEach`; `Kind`, `Problems`, `HasProblems`, `ClearProblems`), `ContentSource` (`file`, `line`, `origin`, `ToText`), `FromFile`, `FromCode`, `kNoLine`, `ContentProblem`, `ContentProblemKind` (`Duplicate`, `Unknown`, `Malformed`) | CNT-1 |

**The ID rule:** `namespace:name`; the namespace `engine` or `game`; the name one or more parts separated by `/`,
each of lowercase ASCII letters, digits and `_` (`engine:test_ability`, `game:rocks/granite_01`); at most 128
characters. `Parse` names the first fault and where it is (`'engine:Test' is not a content ID: 'T' at 7 is not a
lowercase letter, a digit, '_' or '/' between parts`). The hash is 64-bit FNV-1a of the text, the same in every
process, build and platform, so it may be written into cooked files; equality compares the text too.

**The registry rule:** a definition is added with its source. A second definition of an ID is reported with both
sources and the first stays (`item game:test_sword is defined twice: at Data/more_items.json:12, and first at
Data/items.json:4 (kept)`). `Find` reports nothing; `Resolve` is a reference from somewhere, and an ID it cannot find
is reported with where it was referenced (`Data/quests.json:40 names item game:test_swrod, which nothing defines`).
Malformed text is reported with its source wherever it is added or resolved. Problems accumulate in the order found
until cleared. Definitions keep their address while the registry lives (a deque); `ForEach` visits them in the order
they were added.

## Owns

- The content ID format, its namespaces and its hash.
- Sources (a project file and its line, or what made a definition) and the problems' sentences.
- Registries and lookup. Later (CNT-2, slice 20): versioned schemas, validation, loading definitions from the
  project's data files into registries, live reload.

## Does not own

- What a definition is: each module or the game defines its own types and fills a registry of them.
- Asset IDs and the asset cache (Runic::Assets, ASSET-1), which use content IDs for names.
- Reading files: CNT-2's loaders; until then the caller reads and hands over the text and its source.

## Allowed dependencies

Catalog row (ceiling): Core. CNT-1 links Runic::Core (public: `Result` in the API).

## Forbidden dependencies

Every module above Foundation's Core; any third-party library (the hash and the parser are a few lines).

## Threading and tick model

A registry is filled and read on one thread, the loader's, before the simulation reads it; it takes no lock.
Lookups are `const` and safe from several threads once nothing adds. Parsing and hashing are pure functions.

## Data ownership

A registry owns its definitions, their IDs and their sources, and its problems. A `ContentId` owns its text.

## Integration points

- Engine code writes its IDs with `ContentId::Of("engine:...")` (asserts on a typo); data goes through `Parse` or a
  registry's `Add`/`Resolve` with text, so a mistake is a problem with a source, never an assert.
- A cook (the Cooker, COOK-1 on) and a loader (CNT-2) print every registry's problems and fail on any.

## Where the code lives

- `Public/ContentId.hpp`, `Public/ContentRegistry.hpp` (the registry is a template).
- `Source/ContentIds/ContentId.cpp` (parsing, namespaces, the hash); `Source/Registries/ContentRegistry.cpp`
  (sources, problems).
- `Tests/ContentTests.cpp`.

## Work packages

| ID | Package | Done when | Status |
| --- | --- | --- | --- |
| CNT-1 | ContentId with engine: and game: namespaces, registries, lookup | Duplicate and unknown IDs are reported with their source | **Implemented** (slice 9, preview branch): `RunicContentTests` passes with the 9 tests below; awaiting the owner's review |
| CNT-2 | Versioned schemas, validation, loading, live-reload hooks | A changed definition reloads without a restart | Not started (slice 20) |

CNT-1's acceptance tests (`RunicContentTests`): `ContentId.*` (both namespaces parsed; why text is not an ID, with the
fault's position, for an empty text, no namespace, another namespace, no name, an uppercase letter, a space, empty
parts, a second `:`, a text too long; the hash's fixed values, `std::hash`, ordering), `ContentRegistry.*` (found by ID
or text, lookups report nothing, the order added; **a duplicate is reported with both sources and the first stays**;
**an unknown ID is reported with where it was referenced**; a malformed ID with its source; definitions keep their
address over 1,000 additions) and `ContentSource.*`.

Decisions taken in CNT-1 that await the owner (also in the slice brief): two namespaces only (`engine`, `game`); names
of lowercase letters, digits and `_` with `/` between parts, at most 128 characters; 64-bit FNV-1a of the text as the
hash, stable across processes and written into cooked files; a duplicate keeps the first definition; `Find` reports
nothing and `Resolve` reports; a registry takes no lock.

Costs, measured (release, `-O2`, 10,000 definitions): adding one 241 ns (its ID parsed and its source copied), a lookup
by text 51 ns (hashed then compared), by a parsed ID 26 ns, parsing an ID 46 ns.

## Future extensions

- CNT-2: schemas, validation, loading from Data/, live reload; a registry's change notifications for live reload.
- More namespaces if the project allows packs or mods (the owner decided there is no mod support).
