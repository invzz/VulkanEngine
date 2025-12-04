#include "Engine/Core/Mouse.hpp"

#include "Engine/Scene/AnimationController.hpp"

namespace engine {

  std::pair<double, double> Mouse::getCursorPosition() const
  {
    double xPos;
    double yPos;
    glfwGetCursorPos(window.getGLFWwindow(), &xPos, &yPos);
    return {xPos, yPos};
  }

  void Mouse::lookAround(float deltaTime, GameObject& gameObject)
  {
    // If cursor is manually shown (ESC pressed), don't do camera control
    if (window.isCursorVisible())
    {
      // User wants cursor visible - unlock our internal state if needed
      if (cursorLocked_)
      {
        unlockCursor();
      }
      return;
    }

    // Auto-lock behavior when window is focused
    if (!window.isFocused())
    {
      if (cursorLocked_)
      {
        unlockCursor();
      }
      return;
    }

    if (!cursorLocked_)
    {
      lockCursor();
      return;
    }

    double xpos;
    double ypos;
    std::tie(xpos, ypos) = getCursorPosition();

    if (!mouseInitialized_)
    {
      lastX             = xpos;
      lastY             = ypos;
      mouseInitialized_ = true;
      return;
    }

    auto xoffset = static_cast<float>(xpos - lastX) * pixelSensitivity;
    auto yoffset = static_cast<float>(lastY - ypos) * pixelSensitivity;

    lastX = xpos;
    lastY = ypos;

    gameObject.transform.rotation.y += xoffset * lookSpeed * deltaTime;
    gameObject.transform.rotation.x += yoffset * lookSpeed * deltaTime;

    // clamp the pitch rotation to avoid flipping
    gameObject.transform.rotation.x = glm::clamp(gameObject.transform.rotation.x, -1.5f, 1.5f);
    gameObject.transform.rotation.y = glm::mod(gameObject.transform.rotation.y, glm::two_pi<float>());
  }

  void Mouse::reset()
  {
    mouseInitialized_ = false;
    lastX             = 0.0;
    lastY             = 0.0;
  }

  void Mouse::lockCursor()
  {
    cursorLocked_ = true;
    glfwSetInputMode(getGLFWwindow(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    recenterCursor();
  }

  void Mouse::unlockCursor()
  {
    cursorLocked_ = false;
    glfwSetInputMode(getGLFWwindow(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    reset();
  }

  void Mouse::recenterCursor()
  {
    const double centerX = static_cast<double>(window.getWidth()) * 0.5;
    const double centerY = static_cast<double>(window.getHeight()) * 0.5;
    glfwSetCursorPos(getGLFWwindow(), centerX, centerY);
    reset();
  }

} // namespace engine