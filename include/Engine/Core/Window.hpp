#ifndef VULKANENGINE_INCLUDE_ENGINE_CORE_WINDOW_HPP
#define VULKANENGINE_INCLUDE_ENGINE_CORE_WINDOW_HPP

#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

#include <string>

namespace engine {

  class Window
  {
  public:
    Window(int width, int height, const std::string& title);
    ~Window();

    // avoid dangling pointers
    Window(const Window&)            = delete;
    Window& operator=(const Window&) = delete;

    [[nodiscard]] bool        shouldClose() const { return glfwWindowShouldClose(window) != 0; }
    [[nodiscard]] bool        wasWindowResized() const { return framebufferResized; }
    void                      resetWindowResizedFlag() { framebufferResized = false; }
    void                      createWindowSurface(VkInstance instance, VkSurfaceKHR* surface);
    [[nodiscard]] GLFWwindow* getGLFWwindow() const { return window; }
    [[nodiscard]] uint32_t    getWidth() const { return width; }
    [[nodiscard]] uint32_t    getHeight() const { return height; }
    [[nodiscard]] VkExtent2D  getExtent() const { return {width, height}; }
    [[nodiscard]] bool        isFocused() const { return glfwGetWindowAttrib(window, GLFW_FOCUSED) == GLFW_TRUE; }

    // Cursor control
    void               setCursorVisible(bool visible);
    void               toggleCursor();
    [[nodiscard]] bool isCursorVisible() const { return cursorVisible; }

  private:
    static void framebufferResizeCallback(GLFWwindow* window, int width, int height);

    void initWindow();

    GLFWwindow* window;
    uint32_t    width;
    uint32_t    height;

    // Track if GLFW has been initialized
    bool glfwInitialized = false;

    // Flag to indicate if the framebuffer has been resized
    bool framebufferResized = false;

    // Cursor visibility state
    bool cursorVisible = true;

    const std::string title;
  };

} // namespace engine

#endif // VULKANENGINE_INCLUDE_ENGINE_CORE_WINDOW_HPP
