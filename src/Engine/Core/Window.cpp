#include "Engine/Core/Window.hpp"
#include "Engine/Core/Logger.hpp"
#include <chrono>
#include <climits>
#include <string>
#include <thread>
#include <utility>

#include "GLFW/glfw3.h"
#include "vulkan/vulkan_core.h"

#ifdef __linux__
#include <X11/Xlib.h>
#endif

#include "Engine/Core/Exceptions.hpp"


// Forward ImGui GLFW callbacks so the app can choose to install or forward
// events instead of relying on the backend to auto-install them.

// Global GLFW init guard: protects against double-init when multiple Window
// objects are constructed (e.g., static/global objects, test fixtures).
namespace {
    bool g_glfwInitialized = false;

#ifdef __linux__
    // Try to get the global cursor position via X11 (useful for XWayland).
    bool tryGetXCursorPosition(int& outX, int& outY) {
        if (getenv("DISPLAY") == nullptr)
            return false;
        ::Display* dpy = XOpenDisplay(nullptr);
        if (dpy == nullptr)
            return false;

        ::Window     root  = DefaultRootWindow(dpy);
        ::Window     ret   = 0;
        ::Window     child = 0;
        int          rootx = 0;
        int          rooty = 0;
        int          winx  = 0;
        int          winy  = 0;
        unsigned int mask  = 0;
        const Bool   ok    = XQueryPointer(dpy, root, &ret, &child, &rootx, &rooty, &winx, &winy, &mask);
        XCloseDisplay(dpy);
        if (ok == 0)
            return false;
        outX = rootx;
        outY = rooty;
        return true;
    }
#endif

    // Pick a monitor containing the cursor. If haveCursor is false, returns
    // nullptr.
    GLFWmonitor* pickMonitorForCursor(GLFWmonitor** monitors, int monitorCount, int cursorX, int cursorY) {
        for (int i = 0; i < monitorCount; ++i) {
            int mx = 0;
            int my = 0;
            glfwGetMonitorPos(monitors[i], &mx, &my);
            const GLFWvidmode* mode = glfwGetVideoMode(monitors[i]);
            if (mode == nullptr)
                continue;
            int const mw = mode->width;
            int const mh = mode->height;
            if (cursorX >= mx && cursorX < mx + mw && cursorY >= my && cursorY < my + mh) {
                return monitors[i];
            }
        }
        return nullptr;
    }

    // Choose a target monitor given cursor availability; returns primary
    // monitor as fallback.
    GLFWmonitor* chooseTargetMonitor(bool haveCursor, int cursorX, int cursorY) {
        int           monitorCount = 0;
        GLFWmonitor** monitors     = glfwGetMonitors(&monitorCount);
        if (haveCursor) {
            GLFWmonitor* found = pickMonitorForCursor(monitors, monitorCount, cursorX, cursorY);
            if (found != nullptr)
                return found;
        }
        return glfwGetPrimaryMonitor();
    }

    // Wait for the window position to stabilize or become non-zero. Returns
    // last pos.
    void waitForWindowStabilize(GLFWwindow* window, int& outX, int& outY) {
        int       prevX    = INT_MIN;
        int       prevY    = INT_MIN;
        const int maxIters = 100;  // ~1s
        for (int i = 0; i < maxIters; ++i) {
            glfwPollEvents();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            glfwGetWindowPos(window, &outX, &outY);
            if ((outX != 0 || outY != 0) && outX == prevX && outY == prevY)
                break;
            prevX = outX;
            prevY = outY;
        }
    }

    // Request centering on the given monitor (best-effort).
    void centerWindowOnMonitor(GLFWwindow* window, GLFWmonitor* monitor, int width, int height) {
        if (monitor == nullptr)
            return;
        int mx = 0;
        int my = 0;
        glfwGetMonitorPos(monitor, &mx, &my);
        const GLFWvidmode* mode = glfwGetVideoMode(monitor);
        if (mode == nullptr)
            return;
        int const xpos = mx + ((mode->width - width) / 2);
        int const ypos = my + ((mode->height - height) / 2);
        engine::Logger::info(engine::LogChannel::General, "Window on monitor '",
                  ((glfwGetMonitorName(monitor) != nullptr) ? glfwGetMonitorName(monitor) : "unknown"), "' at (",
                  xpos, ", ", ypos, ")");
        glfwSetWindowPos(window, xpos, ypos);
    }

}  // namespace

namespace engine {

    Window::Window(int width, int height, std::string title, bool fullscreen)
        : window(nullptr), width(width), height(height), title(std::move(title)), fullscreen_(fullscreen), prevX(0), prevY(0), prevWidth(static_cast<uint32_t>(width)), prevHeight(static_cast<uint32_t>(height)) {
        initWindow();
    }

    Window::~Window() {
        if (window != nullptr) {
            glfwDestroyWindow(window);
            window = nullptr;
        }
        // Note: we do NOT call glfwTerminate() here. GLFW is a global resource
        // and we cannot safely determine whether this is the last Window. The
        // process cleanup handles termination. If explicit shutdown is needed,
        // call glfwTerminate() in main() after all Windows are destroyed.
    }

    void Window::framebufferResizeCallback(GLFWwindow* window, int width, int height) {
        auto win = static_cast<Window*>(glfwGetWindowUserPointer(window));
        if (win == nullptr)
            return;

        // Record timestamp first (steady_clock for monotonicity), then mark resized.
        uint64_t const nowNs = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
        win->lastResizeTimeNs.store(nowNs);
        win->width.store(static_cast<uint32_t>(width));
        win->height.store(static_cast<uint32_t>(height));
        win->framebufferResized.store(true);

        engine::Logger::info(engine::LogChannel::General, "[Window] framebuffer resized to ", width, "x", height, " (ts=", nowNs, ")");
    }

    void Window::initWindow() {
        if (g_glfwInitialized)
            return;

        // Pre-check: on Linux, GLFW needs X11 or Wayland.
        // Check for Wayland first (preferred on modern Linux), then X11.
#ifdef __linux__
        if (getenv("WAYLAND_DISPLAY") != nullptr) {
            // Wayland display detected — let GLFW handle it.
        } else if (getenv("DISPLAY") == nullptr) {
            throw WindowInitializationException(
                "GLFW initialization failed: no display server found. "
                "Set WAYLAND_DISPLAY (Wayland) or DISPLAY (X11). "
                "For headless mode, set GLFW_PLATFORM to GLFW_PLATFORM_EGL or "
                "use a virtual framebuffer (xvfb-run).");
        } else {
            // X11 display set — verify connectivity.
            ::Display* dpy = XOpenDisplay(nullptr);
            if (dpy == nullptr) {
                throw WindowInitializationException(
                    "GLFW initialization failed: cannot connect to X11 display '" +
                    std::string(getenv("DISPLAY")) +
                    "'. Verify the display server is "
                    "running (e.g., 'xset q' or 'xdpyinfo').");
            }
            XCloseDisplay(dpy);
        }
#endif

        // Prefer Wayland over X11 on Linux if both are available.
#ifdef __linux__
        if (getenv("WAYLAND_DISPLAY") != nullptr) {
            glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_WAYLAND);
        }
#endif

        if (glfwInit() == 0) {
            throw WindowInitializationException("GLFW initialization failed");
        }
        g_glfwInitialized = true;

        // Basic GLFW hints: no GL context, resizable, create hidden so we can
        // position before showing.
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
        glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);

        // Try to pick the monitor where the user likely wants the window.
        int  cursorX    = 0;
        int  cursorY    = 0;
        bool haveCursor = false;

#ifdef __linux__
        haveCursor = tryGetXCursorPosition(cursorX, cursorY);
#endif

        GLFWmonitor* targetMonitor = chooseTargetMonitor(haveCursor, cursorX, cursorY);

        // Create the window (hidden)
        const GLFWvidmode* mode = nullptr;
        if (targetMonitor != nullptr)
            mode = glfwGetVideoMode(targetMonitor);

        if (fullscreen_) {
            // Exclusive fullscreen: create on the monitor using its current mode.
            if (mode != nullptr) {
                glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
                window = glfwCreateWindow(mode->width, mode->height, title.c_str(), targetMonitor, nullptr);
            } else {
                window = glfwCreateWindow(width, height, title.c_str(), targetMonitor, nullptr);
            }
        } else {
            window = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
        }

        if (window == nullptr) {
            throw WindowCreationException("Failed to create GLFW window");
        }

        // Setup user pointer and callbacks
        glfwSetWindowUserPointer(window, this);
        glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);

        // Input callbacks are now installed by ImGui (we initialize ImGui with
        // install_callbacks=true). If you need custom app-level callbacks, install
        // them and call through to ImGui's handlers (e.g. ImGui_ImplGlfw_KeyCallback)
        // to keep ImGui input working.

        // If we have a target monitor, compute centered position and request
        // it. Note: on Wayland compositors (Hyperland) the compositor may
        // ignore this request.
        if (targetMonitor != nullptr) {
            int mx = 0;
            int my = 0;
            glfwGetMonitorPos(targetMonitor, &mx, &my);
            const GLFWvidmode* mode = glfwGetVideoMode(targetMonitor);
            if (mode != nullptr) {
                int const xpos = mx + ((mode->width - width) / 2);
                int const ypos = my + ((mode->height - height) / 2);

                auto monitorName = glfwGetMonitorName(targetMonitor);
                engine::Logger::info(engine::LogChannel::General, "Window position (",
                          xpos, ", ", ypos, ")");
                glfwSetWindowPos(window, xpos, ypos);
            }
        }

        // Wayland compositors sometimes ignore our initial placement request if
        // made immediately after creation, so wait briefly for the compositor
        // to react.
        int posX = 0;
        int posY = 0;
        waitForWindowStabilize(window, posX, posY);

        // Show the window now that we've attempted to position it.
        glfwShowWindow(window);

        // If the compositor still left us at (0, 0), try centering manually.
        if (targetMonitor != nullptr) {
            glfwGetWindowPos(window, &posX, &posY);
            if (posX == 0 && posY == 0) {
                centerWindowOnMonitor(window, targetMonitor, width, height);
            }
        }
    }

    void Window::createWindowSurface(VkInstance instance, VkSurfaceKHR* surface) {
        if (glfwCreateWindowSurface(instance, window, nullptr, surface) != VK_SUCCESS) {
            throw WindowSurfaceCreationException("failed to create window surface!");
        }
    }

    void Window::setCursorMode(bool navigation) {
        cursorNavigationMode_ = navigation;
        cursorVisible         = !navigation;
        glfwSetInputMode(window, GLFW_CURSOR, navigation ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
    }

    void Window::setCursorVisible(bool visible) {
        cursorVisible         = visible;
        cursorNavigationMode_ = false;
        glfwSetInputMode(window, GLFW_CURSOR, visible ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_HIDDEN);
    }

    void Window::toggleCursor() {
        setCursorVisible(!cursorVisible);
    }

    void Window::setFullscreen(bool enabled) {
        if (enabled == fullscreen_)
            return;

        GLFWmonitor* monitor = nullptr;
        int          mx = 0, my = 0;
        if (enabled) {
            // store previous windowed pos/size
            glfwGetWindowPos(window, &prevX, &prevY);
            prevWidth  = getWidth();
            prevHeight = getHeight();

            monitor                 = chooseTargetMonitor(true, prevX + (prevWidth / 2), prevY + (prevHeight / 2));
            const GLFWvidmode* mode = monitor ? glfwGetVideoMode(monitor) : nullptr;
            if (mode) {
                // switch to exclusive fullscreen using monitor's mode
                glfwSetWindowMonitor(window, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
                glfwSetWindowAttrib(window, GLFW_DECORATED, GLFW_FALSE);
                width.store(static_cast<uint32_t>(mode->width));
                height.store(static_cast<uint32_t>(mode->height));
            }
        } else {
            // restore windowed mode
            glfwSetWindowMonitor(window, nullptr, prevX, prevY, static_cast<int>(prevWidth), static_cast<int>(prevHeight), 0);
            glfwSetWindowAttrib(window, GLFW_DECORATED, GLFW_TRUE);
            width.store(prevWidth);
            height.store(prevHeight);
        }

        fullscreen_ = enabled;
    }
}  // namespace engine
