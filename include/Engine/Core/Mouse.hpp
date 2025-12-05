#pragma once
#include <GLFW/glfw3.h>

#include <utility>

#include "Engine/Core/Window.hpp"

namespace engine {
  class Mouse
  {
  public:
    explicit Mouse(Window& window) : window{window} {}
    ~Mouse() = default;

    std::pair<double, double> getCursorPosition() const;

    void lookAround(float deltaTime, struct TransformComponent& transform);

    void reset();

  private:
    void        lockCursor();
    void        unlockCursor();
    void        recenterCursor();
    GLFWwindow* getGLFWwindow() const { return window.getGLFWwindow(); }

    Window& window;
    float   lookSpeed         = 1.5f;           // scalar multiplier for look sensitivity
    float   pixelSensitivity  = 45.0f / 180.0f; // converts pixel delta to radians
    double  lastX             = 0.0;
    double  lastY             = 0.0;
    bool    mouseInitialized_ = false;
    bool    cursorLocked_     = false;
  };
} // namespace engine