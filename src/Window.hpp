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
        VkExtent2D getExtent() const { return {width, height}; }
        void       createWindowSurface(VkInstance instance, VkSurfaceKHR* surface);

      private:
        void initWindow();

        GLFWwindow*    window;
        const uint32_t width;
        const uint32_t height;
        std::string    title;
    };

} // namespace engine