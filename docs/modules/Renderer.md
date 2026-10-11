# Renderer

Contract seed for `Runic::Renderer`: what a Claude Code session needs to implement the Renderer work packages. When `Engine/Presentation/Renderer/` is created, this file becomes its `ARCHITECTURE.md`, and that file is authoritative from then on.

| Field | Value |
| --- | --- |
| Group | Presentation |
| Directory | `Engine/Presentation/Renderer/` |
| CMake target | `Runic::Renderer` |
| Namespace | `Runic::Renderer` |
| Headless | no |
| Status | Not started |

## Purpose

The Renderer draws an extracted scene and never becomes game logic. It owns the bgfx device and views, the layered debug pipeline, render targets for diegetic UI, the batched world-UI feature and, later, the production pipeline (ARCH § Debug renderer; ARCH § Render data, extraction and world UI). **Core rule:** simulation owns meaning; the Renderer owns visualization. Flecs, Jolt, combat, networking and gameplay systems never issue bgfx commands.

## Owns

- The bgfx device, views, a camera with a backend-correct projection, the vertex-color debug shader and the line batch (RND-1).
- Debug shapes, filled shapes with opacity, depth and overlay batches, and debug text (RND-2).
- The debug pipeline: one layer per kind of render data, drawn simply and toggled from the ImGui overlay or the Editor (RND-3 onward).
- Render targets and world quads for diegetic panels (RND-9) and the batched world-UI feature: instanced labels and bars from an atlas (RND-10).
- The production pipeline, later: materials, shaders, lighting and shadows, terrain, water shading, VFX, decals and the UI renderer (RND-6, slice 44+).

| Debug layer | Draws | Slice | Package |
| --- | --- | --- | --- |
| Debug shapes | Lines, shapes and text issued by any system | 2 | RND-2 |
| Physics geometry | Collision bodies and shapes extracted by Physics, with cached debug meshes for large shapes | 8 | RND-3 |
| Meshes | Cooked mesh instances, flat-shaded | 22 | RND-4 |
| Skinned meshes | Characters with their poses, flat-shaded | 23 | RND-5 |
| Lights, environment, volumes | Light gizmos and simple lighting, skylight and darkness, fog, darkness zones | 25a | RND-7 |
| Water and fields | Flat water surfaces; currents and wind as arrows, streaks or heatmaps | 42a | RND-8 |

Debug renderer rules (ARCH § Debug renderer):

- One bgfx view and one tiny vertex-color shader to start; every wire primitive becomes line-segment vertices.
- Never one draw call per line: each frame appends vertices to a CPU list, copies them into one transient vertex buffer and submits one line list. Very large persistent debug geometry moves to dynamic or static buffers.
- Two batches: WorldDepth (depth-tested) and Overlay (no depth test, usually a later view), so hidden collision bounds can be shown without making every line x-ray. Filled shapes go in a second transient batch with alpha blending and no depth write.
- Segments are a budget: global quality levels (8, 16, 24, 32 segments) and fewer segments for distant or tiny shapes. Debug builds count dropped vertices when transient buffers run out.
- The projection follows the active backend's depth range; the camera stays renderer-independent. All conversion from 64-bit world positions to camera-relative floats happens before anything reaches bgfx.
- Render geometry is never collision truth: combat uses exact math and Jolt queries.
- Growth path: the debug renderer is never thrown away. Later a RenderDevice owns DebugRenderer, WorldRenderer, CharacterRenderer, VFXRenderer and UIRenderer.

| Meaning | Color |
| --- | --- |
| Predicted or local state | Green |
| Authoritative server state | Yellow or orange |
| Damage or harmful volume | Red |
| Healing or beneficial volume | Cyan or green |
| Physics bounds and broadphase | Blue |
| Queries and rays | Magenta or white |
| Inactive or past geometry | Dim gray |

## Does not own

- Physics and simulation state: the Renderer never reads them. Physics fills plain DebugGeometry buffers in Core; the physics view draws them, and neither module links the other.
- Render extraction and the camera's world position (Client); RenderData types (Core).
- The Window and native handles (Application): the Window passes only generic handle data in.
- RmlUi and ImGui integrations and the world-UI decisions (UI).
- Game render features, shaders, art and what debug overlays the Game draws (RunicGame::Client).

## Depends on

| Kind | Allowed |
| --- | --- |
| Engine modules | Core, Assets |
| Third-party (PRIVATE) | bgfx, bx, bimg |

### Must never depend on

- Physics, Simulation, ECS, World or any simulation state.
- Client, UI or Animation (they feed it data).
- bgfx types in `Public/`. bgfx has no approved exception.
- GLFW: native handles arrive as plain data from the Window class.
- RunicPlatform or RunicGame code.

## Public API sketch

ARCH's illustrative debug API, with bgfx kept out of the public header (sketch, fixed in the contract session):

```cpp
// Illustrative. Every bgfx type stays in Source/; the public config uses plain Runic enums.
struct RendererConfig {
    uint32_t width = 1280;
    uint32_t height = 720;
    void* nativeWindowHandle = nullptr;    // copied by the Client from the Window's NativeWindow
    void* nativeDisplayHandle = nullptr;
    NativeWindowType nativeWindowType = NativeWindowType::Default;
    RendererBackend backend = RendererBackend::Vulkan;   // mapped to bgfx in Source/
    bool vsync = true;
};

class Renderer {
public:
    bool Initialize(const RendererConfig& config);
    void Shutdown();
    void Resize(uint32_t width, uint32_t height);
    void BeginFrame(const float* view, const float* projection);
    void EndFrame();

    void DrawLine(Vec3 a, Vec3 b, uint32_t color);
    void DrawRay(Vec3 origin, Vec3 direction, float length, uint32_t color);
    void DrawArrow(Vec3 start, Vec3 end, uint32_t color);
    void DrawAABB(Vec3 min, Vec3 max, uint32_t color);
    void DrawOBB(Vec3 center, Vec3 axisX, Vec3 axisY, Vec3 axisZ, Vec3 halfExtent, uint32_t color);
    void DrawCircle(Vec3 center, Vec3 normal, float radius, uint32_t color, uint32_t segments = 32);
    void DrawSphere(Vec3 center, float radius, uint32_t color, uint32_t segments = 24);
    void DrawCylinder(Vec3 a, Vec3 b, float radius, uint32_t color, uint32_t segments = 24);
    void DrawCapsule(Vec3 a, Vec3 b, float radius, uint32_t color, uint32_t segments = 24);
    void DrawCone(Vec3 apex, Vec3 direction, float length, float halfAngleRadians,
                  uint32_t color, uint32_t segments = 24);
    void DrawArcSector(Vec3 origin, Vec3 forward, Vec3 up, float innerRadius, float outerRadius,
                       float angleRadians, uint32_t color, uint32_t segments = 24);
    void DrawGridXZ(float halfSize, float spacing, uint32_t color);
    void DrawText(uint16_t x, uint16_t y, std::string_view text);
    void DrawDebugGeometry(const RenderData::DebugGeometry& geometry);   // physics view and server shapes
};
```

ARCH also sketches a `DebugShapeCommand` (shape, camera-relative transform, size, color, opacity, filled, depth-tested). The same command stream is what a GameServer's Diagnostics layer sends to an attached Editor.

## Threading and tick rules

- bgfx calls stay at the rendering boundary. Simulation workers write plain debug commands into per-worker queues that are merged before rendering.
- The Renderer reads RenderFrame N while extraction writes N+1, so rendering can run on its own thread.
- The Renderer re-uploads a texture only when its version changes (field grids, water state).
- Debug shaders are compiled offline with shaderc; the build automates it once the proof works.

## Work packages

| ID | Package | Done when | Needs | Slice |
| --- | --- | --- | --- | --- |
| RND-1 | bgfx device, view, camera, vertex-color shader, line batch | A grid and axes draw on Vulkan, the first backend; other backends follow when their shaders are built | APP-2 | 2 |
| RND-2 | Shapes, filled shapes with opacity, depth and overlay batches, debug text | Every debug shape renders batched in one frame | RND-1 | 2 |
| RND-3 | Debug pipeline layer registry and toggles; physics geometry layer with cached debug meshes | Collision shapes line up with gameplay shapes and each layer toggles | RND-2, PHYS-3, CORE-3 | 8 |
| RND-4 | Debug pipeline mesh layer: mesh instances, flat shading, camera-relative | Cooked meshes draw correctly far from the origin | RND-3, ASSET-1, CLI-4 | 22 |
| RND-5 | Debug pipeline skinned layer: skinned meshes with pose upload | An animated character renders | RND-4, CLI-5 | 23 |
| RND-6 | Production pipeline: materials, shaders, lighting and shadows, terrain, water shading, VFX, decals, UI renderer | Defined when a substantial game slice justifies it | RND-4 | 44+ |
| RND-7 | Debug pipeline layers for lights, environment and volumes | Torches, skylight, darkness zones and fog are visible and toggle | RND-4, CLI-7 | 25a |
| RND-8 | Debug pipeline layers for water surfaces and field grids (arrows, streaks, heatmaps) | A river surface and the ocean current draw from cooked data | RND-4, CLI-8 | 42a |
| RND-9 | Render targets and world quads for diegetic panels | A texture rendered offscreen appears on a quad in the world | RND-4 | 23a |
| RND-10 | Batched world-UI feature: instanced labels and bars from an atlas | 500 nameplates draw in one call | RND-9 | 38 |

## Acceptance tests

The Done when column is the acceptance list. The debug renderer's first milestone is a grid, one movable capsule, one camera, one attack arc, one ray, and text showing the simulation tick and network state. Its definition of done: a predicted capsule moves around a grid, the authoritative correction shows separately, attack volumes render with the same dimensions combat uses, and Jolt collision state can be inspected, without any simulation system calling bgfx.

## Open items

- Decided in ARCH revision 369: the public `RendererConfig` uses plain Runic enums (`NativeWindowType`, `RendererBackend`), and the Window class exposes a plain `NativeWindow` that the Client copies into the config. Because both Application and Renderer use `NativeWindowType` and neither links the other, the RND-1 and APP-2 contract sessions ask the owner where it lives (proposal: Core).
- Decided in ARCH revision 369: RND-1 draws on Vulkan, the first backend; other backends follow when their shaders are built.
- Decided in ARCH revision 369: the debug shaders live in `Engine/Presentation/Renderer/Shaders/Debug/`; the RND-1 contract session decides how the build compiles them.
- [OQ-5](../system/11-open-questions.md#oq-5) (decided): no Debug module; the debug pipeline lives here, DebugGeometry in Core, extraction in Physics, inspectors in Editor, the Diagnostics layer in Network.
