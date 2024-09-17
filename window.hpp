#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <string>

namespace mari {
    class Window {
        public:
            Window(int w, int h, std::string name);
            ~Window();

            Window(const Window &) = delete;
            Window &operator=(const Window &) = delete;

            bool shouldClose() { return glfwWindowShouldClose(window); }
            VkExtent2D getExtent() { return {static_cast<uint32_t>(width), static_cast<uint32_t>(height)}; }
            bool wasWindowResized() { return framebufferResized; }
            void resetWindowsResizedFlag() { framebufferResized = false; }
            
            void createWindowSurface(VkInstance instance, VkSurfaceKHR *surface);
        private:
            void initialize();
            static void framebufferResizeCallback(GLFWwindow *w, int width, int height);

            int width;
            int height;
            bool framebufferResized = false;

            std::string name;
            GLFWwindow *window;
    };
}