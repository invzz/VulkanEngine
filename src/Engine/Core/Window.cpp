#include "Engine/Core/Window.hpp"

#include <chrono>
#include <climits>
#include <iostream>
#include <string>
#include <thread>
#include <utility>

#include "GLFW/glfw3.h"
#include "vulkan/vulkan_core.h"

#ifdef __linux__
#include <X11/Xlib.h>
#endif

#include "Engine/Core/Exceptions.hpp"
#include "Engine/Core/ansi_colors.hpp"

// Forward ImGui GLFW callbacks so the app can choose to install or forward
// events instead of relying on the backend to auto-install them.

// Small helpers to keep initWindow simple and readable.
namespace {

#ifdef __linux__
  // Try to get the global cursor position via X11 (useful for XWayland).
  bool tryGetXCursorPosition(int& outX, int& outY)
  {
    if (getenv("DISPLAY") == nullptr) return false;
    ::Display* dpy = XOpenDisplay(nullptr);
    if (dpy == nullptr) return false;

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
    if (ok == 0) return false;
    outX = rootx;
    outY = rooty;
    return true;
  }
#endif

  // Pick a monitor containing the cursor. If haveCursor is false, returns
  // nullptr.
  GLFWmonitor* pickMonitorForCursor(GLFWmonitor** monitors, int monitorCount, int cursorX, int cursorY)
  {
    for (int i = 0; i < monitorCount; ++i)
    {
      int mx = 0;
      int my = 0;
      glfwGetMonitorPos(monitors[i], &mx, &my);
      const GLFWvidmode* mode = glfwGetVideoMode(monitors[i]);
      if (mode == nullptr) continue;
      int const mw = mode->width;
      int const mh = mode->height;
      if (cursorX >= mx && cursorX < mx + mw && cursorY >= my && cursorY < my + mh)
      {
        return monitors[i];
      }
    }
    return nullptr;
  }

  // Choose a target monitor given cursor availability; returns primary
  // monitor as fallback.
  GLFWmonitor* chooseTargetMonitor(bool haveCursor, int cursorX, int cursorY)
  {
    int           monitorCount = 0;
    GLFWmonitor** monitors     = glfwGetMonitors(&monitorCount);
    if (haveCursor)
    {
      GLFWmonitor* found = pickMonitorForCursor(monitors, monitorCount, cursorX, cursorY);
      if (found != nullptr) return found;
    }
    return glfwGetPrimaryMonitor();
  }

  // Wait for the window position to stabilize or become non-zero. Returns
  // last pos.
  void waitForWindowStabilize(GLFWwindow* window, int& outX, int& outY)
  {
    int       prevX    = INT_MIN;
    int       prevY    = INT_MIN;
    const int maxIters = 100; // ~1s
    for (int i = 0; i < maxIters; ++i)
    {
      glfwPollEvents();
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
      glfwGetWindowPos(window, &outX, &outY);
      if ((outX != 0 || outY != 0) && outX == prevX && outY == prevY) break;
      prevX = outX;
      prevY = outY;
    }
  }

  // Request centering on the given monitor (best-effort).
  void centerWindowOnMonitor(GLFWwindow* window, GLFWmonitor* monitor, int width, int height)
  {
    if (monitor == nullptr) return;
    int mx = 0;
    int my = 0;
    glfwGetMonitorPos(monitor, &mx, &my);
    const GLFWvidmode* mode = glfwGetVideoMode(monitor);
    if (mode == nullptr) return;
    int const xpos = mx + ((mode->width - width) / 2);
    int const ypos = my + ((mode->height - height) / 2);
    std::cout << "[" << BLUE << "Window" << RESET << "]" << YELLOW << ((glfwGetMonitorName(monitor) != nullptr) ? glfwGetMonitorName(monitor) : "unknown") << "' at (" << xpos << ", " << ypos << ")"
              << RESET << "\n";
    glfwSetWindowPos(window, xpos, ypos);
  }

} // namespace

namespace engine {

  Window::Window(int width, int height, std::string title) : window(nullptr), width(width), height(height), title(std::move(title))
  {
    initWindow();
  }

  Window::~Window()
  {
    if (window != nullptr)
    {
      glfwDestroyWindow(window);
      window = nullptr;
    }
    if (glfwInitialized)
    {
      glfwTerminate();
      glfwInitialized = false;
    }
  }

  void Window::framebufferResizeCallback(GLFWwindow* window, int width, int height)
  {
    auto win = static_cast<Window*>(glfwGetWindowUserPointer(window));
    if (win == nullptr) return;
    win->framebufferResized = true;
    win->width              = width;
    win->height             = height;
  }

  void Window::initWindow()
  {
    if (glfwInitialized) return;

    if (glfwInit() == 0)
    {
      throw WindowInitializationException("GLFW initialization failed");
    }
    glfwInitialized = true;

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
    window = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
    if (window == nullptr)
    {
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
    if (targetMonitor != nullptr)
    {
      int mx = 0;
      int my = 0;
      glfwGetMonitorPos(targetMonitor, &mx, &my);
      const GLFWvidmode* mode = glfwGetVideoMode(targetMonitor);
      if (mode != nullptr)
      {
        int const xpos = mx + ((mode->width - width) / 2);
        int const ypos = my + ((mode->height - height) / 2);

        auto monitorName = glfwGetMonitorName(targetMonitor);
        std::cout << "[ " << BLUE << "Window" << RESET << " ] " << YELLOW << ((monitorName != nullptr) ? monitorName : "unknown") << BLUE << " position (" << xpos << ", " << ypos << ")" << RESET
                  << "\n";
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
    if (targetMonitor != nullptr)
    {
      glfwGetWindowPos(window, &posX, &posY);
      if (posX == 0 && posY == 0)
      {
        centerWindowOnMonitor(window, targetMonitor, width, height);
      }
    }
  }

  void Window::createWindowSurface(VkInstance instance, VkSurfaceKHR* surface)
  {
    if (glfwCreateWindowSurface(instance, window, nullptr, surface) != VK_SUCCESS)
    {
      throw WindowSurfaceCreationException("failed to create window surface!");
    }
  }

  void Window::setCursorVisible(bool visible)
  {
    cursorVisible = visible;
    glfwSetInputMode(window, GLFW_CURSOR, visible ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED);
  }

  void Window::toggleCursor()
  {
    setCursorVisible(!cursorVisible);
  }

} // namespace engine
