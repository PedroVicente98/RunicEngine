# Engine/Programs

The executables the Engine owns (RunicDocs › Repositories, build targets and processes › Processes). They sit next to
the module groups but are not modules: no catalog row, no `Public/` folder. Each one is a composition root: from
APP-1 (slice 1) on it defines `CreateApplication` for the Engine's entry point and stacks layers that modules supply.

| Program | What | Since |
| --- | --- | --- |
| [RunicEditor](RunicEditor/README.md) | The Godot editor run through LibGodot (editor target), with the Runic tools (Runic::GodotTools) and later the Cooker; never shipped | Slice 0 (GDT-1) |
| [RunicServer](RunicServer/README.md) | The Engine's own game-agnostic server: the Engine's server layers on the fixed 60 Hz loop, neutral test content, no Platform layers; where Engine slices show their server results | APP-1 (slice 1) |
| [RunicClient](RunicClient/README.md) | The Engine's own game-agnostic client: the Engine's client layers with the GodotLayer on top, neutral test content; where Engine slices show their client results | GDB-1 (slice 1) |
| [RunicBot](RunicBot/README.md) | The client runtime without presentation, many bot sessions per process, for scale tests; never shipped | CLI-9 (slice 38) |

The Cooker is not a program: it runs inside the RunicEditor, or as the RunicEditor run headless on build machines.
The game's executables (GameClient and GameServer) are not here: they belong to RunicGame, and they are the same
compositions as RunicServer and RunicClient plus the game's modules (and, on the server, the Platform's layers).

Programs are not modules, so the rule "one work package writes inside one module" would never let a package touch
them. The rule for programs (decided 2026-11-12): a package may also edit the Engine program that shows its result.
APP-1 creates RunicServer; GDB-1 creates RunicClient and moves the RunicEditor onto the entry point. A program joins
the build when its folder has a `CMakeLists.txt` ([`Engine/CMakeLists.txt`](../CMakeLists.txt) adds it: RunicServer
and RunicBot with `RUNIC_ENGINE_BUILD_PROGRAMS`, RunicClient also with presentation, the RunicEditor with the tools).
