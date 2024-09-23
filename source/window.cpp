#include "window.hpp"

#include <stdexcept>

namespace mari {
    Window::Window(int w, int h, std::string name) : width{w}, height{h}, name{name} {
        initialize();
    }

    Window::~Window() {
        glfwDestroyWindow(window);
        glfwTerminate();
    }

    void Window::initialize() {
        glfwInit();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

        window = glfwCreateWindow(width, height, name.c_str(), nullptr, nullptr);
        glfwSetWindowUserPointer(window, this);
        glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
    }

    void Window::createWindowSurface(VkInstance instance, VkSurfaceKHR *surface) {
        if (glfwCreateWindowSurface(instance, window, nullptr, surface) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create window surface.");
        }
    }

    void Window::framebufferResizeCallback(GLFWwindow *w, int width, int height) {
        auto window = reinterpret_cast<Window*>(glfwGetWindowUserPointer(w));
        window->framebufferResized = true;
        window->width = width;
        window->height = height;
    }
}