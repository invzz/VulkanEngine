#pragma once

#include <GLFW/glfw3.h>

#include <string>

namespace engine {

    class Window
    {
      public:
        Window(int width, int height, std::string title);
        ~Window();

        // avoid dangling pointers
        Window(const Window&)            = delete;
        Window& operator=(const Window&) = delete;

        bool       shouldClose() const { return glfwWindowShouldClose(window); }
        bool       wasWindowResized() const { return framebufferResized; }
        void       resetWindowResizedFlag() { framebufferResized = false; }
        void       createWindowSurface(VkInstance instance, VkSurfaceKHR* surface);
        VkExtent2D getExtent() const { return {width, height}; }

      private:
        static void framebufferResizeCallback(GLFWwindow* window, int width, int height);

        void initWindow();

        GLFWwindow* window;
        uint32_t    width;
        uint32_t    height;

        // Flag to indicate if the framebuffer has been resized
        bool framebufferResized = false;

        std::string title;
    };

} // namespace engine