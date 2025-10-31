#include "Window.hpp"

#include <iostream>
#include <stdexcept>

#include "ansi_colors.hpp"

namespace engine {

    Window::Window(int width, int height, std::string title)
        : width(width), height(height), title(title)
    {
        initWindow();
    }

    Window::~Window()
    {
        // Clean up resources
        glfwDestroyWindow(window);
        glfwTerminate();
    }

    void Window::framebufferResizeCallback(GLFWwindow* window, int width, int height)
    {
        auto win                = static_cast<Window*>(glfwGetWindowUserPointer(window));
        win->framebufferResized = true;
        win->width              = width;
        win->height             = height;
    }

    void Window::initWindow()
    {
        // Initialize the GLFW window here
        glfwInit();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

        // Create the window first
        window = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);

        // Set the user pointer to this instance
        glfwSetWindowUserPointer(window, this);

        // Set the framebuffer size callback
        glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);

        if (!window) return;
    }

    void Window::createWindowSurface(VkInstance instance, VkSurfaceKHR* surface)
    {
        if (auto result = glfwCreateWindowSurface(instance, window, nullptr, surface) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create window surface!");
        }
    }

} // namespace engine