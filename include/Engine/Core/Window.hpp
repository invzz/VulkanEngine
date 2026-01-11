#ifndef VULKANENGINE_INCLUDE_ENGINE_CORE_WINDOW_HPP
#define VULKANENGINE_INCLUDE_ENGINE_CORE_WINDOW_HPP

#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

#include <atomic>
#include <chrono>
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

    [[nodiscard]] bool        shouldClose() const { return glfwWindowShouldClose(window) != 0; }
    [[nodiscard]] bool        wasWindowResized() const { return framebufferResized.load(); }
    void                      resetWindowResizedFlag() { framebufferResized.store(false); }
    void                      createWindowSurface(VkInstance instance, VkSurfaceKHR* surface);
    [[nodiscard]] GLFWwindow* getGLFWwindow() const { return window; }
    [[nodiscard]] uint32_t    getWidth() const { return width.load(); }
    [[nodiscard]] uint32_t    getHeight() const { return height.load(); }
    [[nodiscard]] VkExtent2D  getExtent() const { return {width.load(), height.load()}; }
    [[nodiscard]] bool        isFocused() const { return glfwGetWindowAttrib(window, GLFW_FOCUSED) == GLFW_TRUE; }

    // Atomically consume the resized flag: returns previous value and clears it.
    [[nodiscard]] bool consumeWindowResized() { return framebufferResized.exchange(false); }

    // Last resize event timestamp in nanoseconds since steady_clock epoch.
    [[nodiscard]] uint64_t getLastResizeTimeNs() const { return lastResizeTimeNs.load(); }

    // Check whether the last resize is stable for at least `debounceMs` milliseconds.
    [[nodiscard]] bool isResizeStable(uint64_t debounceMs) const
    {
      uint64_t const last = lastResizeTimeNs.load();
      if (last == 0) return false;
      uint64_t const now       = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
      uint64_t const elapsedNs = now - last;
      return elapsedNs >= (debounceMs * 1000000ULL);
    }

    // Cursor control
    void               setCursorVisible(bool visible);
    void               toggleCursor();
    [[nodiscard]] bool isCursorVisible() const { return cursorVisible; }

  private:
    static void framebufferResizeCallback(GLFWwindow* window, int width, int height);

    void initWindow();

    GLFWwindow* window;

    // Track if GLFW has been initialized
    bool glfwInitialized = false;

    // Flag to indicate if the framebuffer has been resized (atomic for callback/thread-safety)
    std::atomic<bool> framebufferResized{false};

    // Last resize timestamp (nanoseconds since steady_clock epoch)
    std::atomic<uint64_t> lastResizeTimeNs{0};

    // Cursor visibility state
    bool cursorVisible = true;

    // Atomic width/height to avoid data races with GLFW callback thread
    std::atomic<uint32_t> width;
    std::atomic<uint32_t> height;

    const std::string title;
  };

} // namespace engine

#endif // VULKANENGINE_INCLUDE_ENGINE_CORE_WINDOW_HPP
