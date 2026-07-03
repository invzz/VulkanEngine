#ifndef VULKANENGINE_INCLUDE_ENGINE_CORE_WINDOW_HPP
#define VULKANENGINE_INCLUDE_ENGINE_CORE_WINDOW_HPP

#include <vulkan/vulkan.h>

#include <GLFW/glfw3.h>
#include <atomic>
#include <chrono>
#include <string>

namespace engine {

    class Window {
       public:
        Window(int width, int height, std::string title, bool fullscreen = false);
        ~Window();

        Window(const Window&)            = delete;
        Window& operator=(const Window&) = delete;

        [[nodiscard]] bool shouldClose() const {
            return glfwWindowShouldClose(window) != 0;
        }
        [[nodiscard]] bool wasWindowResized() const {
            return framebufferResized.load();
        }
        void resetWindowResizedFlag() {
            framebufferResized.store(false);
        }
        void                      createWindowSurface(VkInstance instance, VkSurfaceKHR* surface);
        [[nodiscard]] GLFWwindow* getGLFWwindow() const {
            return window;
        }
        [[nodiscard]] uint32_t getWidth() const {
            return width.load();
        }
        [[nodiscard]] uint32_t getHeight() const {
            return height.load();
        }
        [[nodiscard]] VkExtent2D getExtent() const {
            return {width.load(), height.load()};
        }
        [[nodiscard]] bool isFocused() const {
            return glfwGetWindowAttrib(window, GLFW_FOCUSED) == GLFW_TRUE;
        }

        [[nodiscard]] bool consumeWindowResized() {
            return framebufferResized.exchange(false);
        }

        [[nodiscard]] uint64_t getLastResizeTimeNs() const {
            return lastResizeTimeNs.load();
        }

        [[nodiscard]] bool isResizeStable(uint64_t debounceMs) const {
            uint64_t const last = lastResizeTimeNs.load();
            if (last == 0)
                return false;
            uint64_t const now       = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
            uint64_t const elapsedNs = now - last;
            return elapsedNs >= (debounceMs * 1000000ULL);
        }

        void               setCursorMode(bool navigation);
        void               setCursorVisible(bool visible);
        void               toggleCursor();
        [[nodiscard]] bool isCursorVisible() const {
            return cursorVisible;
        }
        [[nodiscard]] bool isCursorNavigationMode() const {
            return cursorNavigationMode_;
        }

        void setFullscreen(bool enabled);
        void toggleFullscreen() {
            setFullscreen(!isFullscreen());
        }
        [[nodiscard]] bool isFullscreen() const {
            return fullscreen_;
        }

       private:
        static void framebufferResizeCallback(GLFWwindow* window, int width, int height);

        void initWindow();

        GLFWwindow* window;

        std::atomic<bool> framebufferResized{false};

        std::atomic<uint64_t> lastResizeTimeNs{0};

        bool cursorVisible         = true;
        bool cursorNavigationMode_ = false;

        std::atomic<uint32_t> width;
        std::atomic<uint32_t> height;

        bool     fullscreen_ = false;
        int      prevX       = 0;
        int      prevY       = 0;
        uint32_t prevWidth   = 0;
        uint32_t prevHeight  = 0;

        const std::string title;
    };

}  // namespace engine

#endif
