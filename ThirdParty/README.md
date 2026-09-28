# ThirdParty sources

All pins, download hashes, library options, include paths, and link relationships
are managed by [Dependencies.cmake](../cmake/Dependencies.cmake).

- Headless: `flecs/`, `glm/`, `JoltPhysics/`
- Presentation: `glfw/`, `imgui/`, `bgfx/`, `bx/`, `bimg/`, `bgfx.cmake/`

Existing directories are reused. Missing sources are downloaded automatically on
configure; generated objects and libraries stay under the selected `build/`
directory. Downloaded trees are ignored by Git and retain upstream license files.
Do not edit these source trees. Update a pin/hash and remove the corresponding old
source directory to fetch a different revision. Configure build trees sequentially.

X11 and OpenGL development libraries come from the operating system.
The engine's Debug/Release sandbox verifies all dependency headers and linker
inputs without starting a window or renderer.
