#include "renderer.hpp"

namespace mari {
    void Renderer::run() {
        while (!window.shouldClose()) {
            glfwPollEvents();
        }
    };
}