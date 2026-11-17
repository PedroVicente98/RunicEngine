# Source

{{GAME_NAME}}'s C++, compiled before it runs; Godot never executes it (`.gdignore` keeps it out of Godot's importer).

| Folder | Becomes | Runs in |
| --- | --- | --- |
| `Shared/` | Rules and shared Flecs systems, compiled into both executables | The server with authority, the client for prediction |
| `Server/` | AI, quests, loot, server rules | Server only |
| `Client/` | View models, game presentation | Client only |
| `Editor/` | The game's marker nodes and docks: the editor library | Editor only |
