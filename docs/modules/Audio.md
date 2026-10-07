# Audio

> **Contract seed.** When `Engine/Audio/` is created, copy this file to
> `Engine/Audio/ARCHITECTURE.md`; from then on that file is authoritative.
> Purpose, Owns, Does not own, Allowed dependencies, and Primary consumers come
> from Appendix J §J.4.14. Other sections are derived and are proposals until
> confirmed.

| Field | Value |
| --- | --- |
| Directory | `Engine/Audio/` |
| CMake target | `RunicAudio` → `Runic::Audio` |
| Namespace | `Runic::Audio` |
| Roadmap phase | 2 — stubs as needed |
| Headless | **no** |
| Status | Not started — backend library not chosen |

## Purpose

Reusable spatial and non-spatial audio playback abstraction, with the backend
hidden behind Engine APIs.

## Owns

`AudioSystem`, sources, listeners, audio events, buses/handles as required.

## Does not own

Game music/SFX selection rules; gameplay authority.

## Allowed dependencies

- Core, Assets.
- Audio backend — **private**.

## Forbidden dependencies

Simulation mutation, gameplay modules, RunicPlatform, RunicGame. Must not be
linked by the headless server ("client audio" is excluded). [J.14.6]

## Primary consumers

Client, Game presentation, UI.

## Public API (conceptual)

```text
Audio/
├── Public/
│   └── Audio.hpp            # split into explicit headers as the API grows
├── Source/
├── Tests/
└── CMakeLists.txt
```

## Threading / tick model (derived)

Client-side; mixing on the backend's audio thread; API calls from the main
thread.

## Integration points (derived)

Game presentation maps facts/presentation cues to sounds; UI plays interface
sounds; Assets supplies audio data.

## Acceptance tests (Stage A, derived)

- [ ] Play a non-spatial sound and a spatial sound relative to a listener.
- [ ] Backend headers absent from `Public/`.
- [ ] Not linked by the headless server target.

## Open questions

- Backend choice (e.g. miniaudio, OpenAL Soft, FMOD/Wwise) is undecided.
