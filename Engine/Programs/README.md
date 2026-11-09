# Engine/Programs

The executables the Engine owns (RunicDocs › Repositories, build targets and processes › Processes). They sit next to
the module groups but are not modules: no catalog row, no `Public/` folder. Each one composes modules and is the
place a process starts.

| Program | What | Since |
| --- | --- | --- |
| `RunicEditor/` | The Godot editor run through LibGodot, with the Runic tools (Runic::GodotTools); never shipped | Slice 0 (GDT-1) |
| `RunicBot/` | The client runtime without presentation, many bot sessions per process, for scale tests | CLI-9 (slice 38) |

The game's executables (GameClient and GameServer) are not here: they belong to RunicGame.
