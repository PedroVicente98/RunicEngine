# Runtime boundary

`Runic::Runtime` currently only groups headless dependencies. Future application
and fixed-tick simulation code belongs here. Simulation time must remain separate
from render-frame timing. Runtime must not acquire GLFW, bgfx, ImGui, or editor
dependencies. A future LayerStack orders runtime updates/events; CMake targets
define compile-time dependency boundaries.
