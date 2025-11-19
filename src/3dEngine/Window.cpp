#include "3dEngine/Window.hpp"

#include <algorithm>
#include <chrono>
#include <climits>
#include <cstdlib>
#include <iostream>
#include <thread>
#include <vector>

#ifdef __linux__
#include <X11/Xlib.h>
#endif

#include "3dEngine/Exceptions.hpp"
#include "3dEngine/ansi_colors.hpp"

// Small helpers to keep initWindow simple and readable.
namespace window_detail {

#ifdef __linux__
  // Try to get the global cursor position via X11 (useful for XWayland).
  bool tryGetXCursorPosition(int& outX, int& outY)
  {
    if (!getenv("DISPLAY")) return false;
    ::Display* dpy = XOpenDisplay(nullptr);
    if (!dpy) return false;

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
    if (!ok) return false;
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
      if (!mode) continue;
      int mw = mode->width;
      int mh = mode->height;
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
      if (found) return found;
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
    if (!monitor) return;
    int mx = 0;
    int my = 0;
    glfwGetMonitorPos(monitor, &mx, &my);
    const GLFWvidmode* mode = glfwGetVideoMode(monitor);
    if (!mode) return;
    int xpos = mx + (mode->width - width) / 2;
    int ypos = my + (mode->height - height) / 2;
    std::cout << "[" << BLUE << "Window" << RESET << "]" << YELLOW << (glfwGetMonitorName(monitor) ? glfwGetMonitorName(monitor) : "unknown") << "' at ("
              << xpos << ", " << ypos << ")" << RESET << "\n";
    glfwSetWindowPos(window, xpos, ypos);
  }

} // namespace window_detail

namespace engine {

  Window::Window(int width, int height, const std::string& title)
      : window(nullptr), width(width), height(height), glfwInitialized(false), framebufferResized(false), title(title)
  {
    initWindow();
  }

  Window::~Window()
  {
    if (window)
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
    if (!win) return;
    win->framebufferResized = true;
    win->width              = width;
    win->height             = height;
  }

  void Window::initWindow()
  {
    if (glfwInitialized) return;

    if (!glfwInit())
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
    haveCursor = window_detail::tryGetXCursorPosition(cursorX, cursorY);
#endif

    GLFWmonitor* targetMonitor = window_detail::chooseTargetMonitor(haveCursor, cursorX, cursorY);

    // Create the window (hidden)
    window = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
    if (!window)
    {
      throw WindowCreationException("Failed to create GLFW window");
    }

    // Setup user pointer and callbacks
    glfwSetWindowUserPointer(window, this);
    glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);

    // If we have a target monitor, compute centered position and request
    // it. Note: on Wayland compositors (Hyperland) the compositor may
    // ignore this request.
    if (targetMonitor)
    {
      int mx = 0;
      int my = 0;
      glfwGetMonitorPos(targetMonitor, &mx, &my);
      const GLFWvidmode* mode = glfwGetVideoMode(targetMonitor);
      if (mode)
      {
        int xpos = mx + (mode->width - width) / 2;
        int ypos = my + (mode->height - height) / 2;

        auto monitorName = glfwGetMonitorName(targetMonitor);
        std::cout << "[ " << BLUE << "Window" << RESET << " ] " << YELLOW << (monitorName ? monitorName : "unknown") << BLUE << " position (" << xpos << ", "
                  << ypos << ")" << RESET << "\n";
        glfwSetWindowPos(window, xpos, ypos);
      }
    }

    // Wayland compositors sometimes ignore our initial placement request if
    // made immediately after creation, so wait briefly for the compositor
    // to react.
    int posX = 0;
    int posY = 0;
    window_detail::waitForWindowStabilize(window, posX, posY);

    // Show the window now that we've attempted to position it.
    glfwShowWindow(window);

    // If the compositor still left us at (0, 0), try centering manually.
    if (targetMonitor)
    {
      glfwGetWindowPos(window, &posX, &posY);
      if (posX == 0 && posY == 0)
      {
        window_detail::centerWindowOnMonitor(window, targetMonitor, width, height);
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
