// Calls that prove linkage without creating a window, renderer, or UI context.
#include <GLFW/glfw3.h>
#include <bgfx/bgfx.h>
#include <imgui.h>

int main() {
    int major = 0;
    glfwGetVersion(&major, nullptr, nullptr);
    const auto renderers = bgfx::getSupportedRenderers(0, nullptr);
    return major == 3 && renderers > 0 && IMGUI_CHECKVERSION() ? 0 : 1;
}
