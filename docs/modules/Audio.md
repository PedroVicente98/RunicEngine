---
# obsidian-graph: generated tags and aliases
tags:
  - repo/engine
  - source/arch
  - type/module
  - group/presentation
  - headless/no
  - slice/24
aliases:
  - "Runic::Audio"
---

# Audio

Contract seed for `Runic::Audio`: what a Claude Code session needs to implement the Audio work packages. When `Engine/Presentation/Audio/` is created, this file becomes its `ARCHITECTURE.md`, and that file is authoritative from then on.

| Field | Value |
| --- | --- |
| Group | Presentation |
| Directory | `Engine/Presentation/Audio/` |
| CMake target | `Runic::Audio` |
| Namespace | `Runic::Audio` |
| Headless | no |
| Status | Not started |

## Purpose

Audio is client presentation only. The simulation decides what happened, the game turns it into semantic audio events and music contexts, and this module plays them. Players may replace the soundtrack, never sound effects, and nothing they choose can change gameplay (ARCH[^arch] § Audio).

## Owns

- The backend abstraction, sources and handles, buses and mixing, and semantic event routing through an AudioEventRegistry (AUD-1).
- The MusicDirector: music contexts, fallback chains, albums, soundtrack presets, radio-style playback, local profile persistence, playback state, crossfades and transitions (AUD-2).

Semantic sound events:

- Presentation code never names a file; it plays a semantic event that the registry resolves to a route and then to a source.
- Developer features: random, weighted, no-repeat and sequential variants; material-specific routing; pitch and volume ranges; spatial or non-spatial playback; bus assignment; live reload in development; debug inspection.
- Not player-remappable: routing exists for content production, not as a modding surface.

Music:

- A music context names a musical situation, not a track. The game owns the set and may grow it.
- Fallback chains keep old presets valid when contexts are added. Priority between valid contexts is game presentation policy.
- A preset maps contexts to albums. The game's original soundtrack is itself a built-in, read-only preset using the same machinery; user presets can be created, duplicated, renamed, imported, exported and deleted.
- Each album keeps its own playback state, so switching contexts suspends a station instead of restarting it.

| Album setting | Options |
| --- | --- |
| Playback | Sequential, Shuffle, ShuffleNoRepeat |
| Transition | Immediate, Crossfade, FinishCurrentTrack |
| Resume | ResumePreviousPosition, RestartCurrentTrack, StartNextTrack |

Security and failure rules:

- Soundtrack content is never executable: presets cannot load DLLs, scripts, codec plugins or native extensions disguised as audio. Remote streaming URLs are out of scope.
- The server never stores or sends player-local file paths, preset names or playback state; no Platform service is involved.
- Music reacts only to facts the client may already know; no cue may reveal what the visibility tiers hide.
- A missing or unreadable track falls back in order: the next valid track in the album, then the fallback context's album, then the default soundtrack. A broken preset never crashes the client or touches gameplay.
- Importing a preset validates its schema version, context IDs, track references, playback modes and fallback configuration, and rejects executable payloads. Unknown future contexts fall back through the chain instead of failing the load.

## Does not own

- Concrete music contexts, default assignments, sound-event definitions, context priority, bundled music and sounds, and the soundtrack settings screens (RunicGame).
- Deciding what happened: presentation turns simulation facts and cues into events.
- Any server role: the GameServer has no audio backend, decoders, profiles or soundtrack UI.

## Depends on

| Kind | Allowed |
| --- | --- |
| Engine modules | Core[^engine-core], Assets[^engine-assets] |
| Third-party (PRIVATE) | An audio backend and decoders, not chosen yet |

### Must never depend on

- Combat[^engine-combat], AI, quests, simulation authority, Platform networking or persistence services (ARCH[^arch] § Audio).
- Any headless build or server target.
- Backend types in `Public/`.
- RunicPlatform or RunicGame code.

## Public API sketch

ARCH[^arch]'s illustrative contract (ARCH § Audio), with a neutral event ID:

```cpp
// Avoid: PlaySound("Audio/Combat/Hit03.wav");
audio.PlayEvent("engine:test_event");

struct MusicAlbum {
    AlbumId id;
    std::vector<MusicTrack> tracks;
    PlaybackMode playback_mode;
    TransitionMode transition_mode;
    ResumeMode resume_mode;
};

struct AlbumPlaybackState {
    TrackId current_track;
    uint32_t playlist_index;
    double playback_position_seconds;
    uint64_t shuffle_state;
};

class MusicDirector {
public:
    void SetContext(MusicContextId context);
    void SetActivePreset(SoundtrackPresetId preset);
    MusicContextId CurrentContext() const;
    SoundtrackPresetId ActivePreset() const;
};
```

Headers (sketch, fixed in the contract session): `AudioBackend.hpp`, `AudioBus.hpp`, `AudioEventRegistry.hpp`, `MusicDirector.hpp`, `MusicAlbum.hpp`, `SoundtrackPreset.hpp`, `AudioProfile.hpp`.

## Threading and tick rules

- Audio runs only in client and tools processes, driven by the client's Presentation layer.
- Audio never reads or writes simulation state; it plays what presentation asks for, from facts the client already knows.
- Developer tooling shows the current context and its fallback chain, the active preset, album, track and position, transition state, bus volumes, recent events, how each resolved, and missing mappings or files.

## Work packages

| ID | Package | Done when | Needs | Slice |
| --- | --- | --- | --- | --- |
| AUD-1 | Audio backend abstraction, buses, mixing, semantic sound events | A sound event plays through its bus with volume control | ASSET-1[^engine-assets] | 24 |
| AUD-2 | MusicDirector: contexts, fallback chains, albums, presets, radio playback, local profile | A context change switches album and remembers the position | AUD-1 | 24 |

## Acceptance tests

| Test | Passes when |
| --- | --- |
| Default soundtrack | With no custom profile, the built-in default preset is active and every context resolves to its defaults with no backend service |
| Custom preset | Separate local albums for two contexts switch with the configured transition and resume from stored state |
| Radio behavior | A suspended album resumes at its stored track and position after another context ends, and after a client restart as the policy says |
| Context compatibility | An old preset without a new context resolves it through fallback and stays editable |
| Broken local track | A deleted file is skipped or falls back without a crash, and the tooling reports it |
| Sound-effect boundary | An event resolves through the registry, has no player remapping UI, and is inspectable in tooling |
| Headless boundary | The server builds with no audio backend or soundtrack dependency, and gameplay is identical whatever soundtrack clients use |

Tests use Engine test events, contexts and tracks.

## Open items

- The audio backend and decoder libraries are not chosen; the AUD-1 contract session asks the owner and adds the pin to `cmake/Dependencies.cmake`.

<!-- obsidian-graph:start -->

---

## Connections

*Generated for the Obsidian graph. The numbered markers in the text jump to the footnotes below, which link the notes they refer to.*

- **Depends on:** [[../modules/Core|Core]] · [[../modules/Assets|Assets]]
- **Depended on by:** [[../modules/Client|Client]]
- **Packages that need it:** [[../../../RunicGame/docs/game-packages|Game work packages]]
- **See also:** [[../modules/README|Engine module catalog]] · [[../system/03-dependency-rules|03 — Dependency rules]] · [[../system/12-work-packages|12 — Work packages]] · [[../current-state|Engine current state]]

[^arch]: #source/arch — ARCH, "RunicEngine — Technical Architecture v3": the architecture source of truth (a project document, not a note in this vault)
[^engine-core]: [[../modules/Core|Core]] — Engine module · Foundation
[^engine-assets]: [[../modules/Assets|Assets]] — Engine module · Foundation
[^engine-combat]: [[../modules/Combat|Combat]] — Engine module · GameplayAPI
