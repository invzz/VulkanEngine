#include "Window.hpp"

#include <stdexcept>

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

    void Window::initWindow()
    {
        // Initialize the GLFW window here
        glfwInit();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

        // Get the primary monitor and its video mode
        GLFWmonitor*       monitor = glfwGetPrimaryMonitor();
        const GLFWvidmode* mode    = glfwGetVideoMode(monitor);

        // Center the window on the current monitor
        int xpos = (mode->width - width) / 2;
        int ypos = (mode->height - height) / 2;
        window   = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
        if (window)
        {
            glfwSetWindowPos(window, xpos, ypos);
        }
    }

    void Window::createWindowSurface(VkInstance instance, VkSurfaceKHR* surface)
    {
        if (auto result = glfwCreateWindowSurface(instance, window, nullptr, surface) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create window surface!");
        }
    }

} // namespace engine