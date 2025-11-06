#pragma once

#include <GLFW/glfw3.h>

#include "GameObject.hpp"
#include "Window.hpp"

namespace engine {

  class Keyboard
  {
  public:
    explicit Keyboard(Window& window) : windowRef{window} {}

    struct KeyMappings
    {
      // Wasd keys for movement
      int moveForward  = GLFW_KEY_W;
      int moveBackward = GLFW_KEY_S;
      int moveLeft     = GLFW_KEY_A;
      int moveRight    = GLFW_KEY_D;
      int moveUp       = GLFW_KEY_SPACE;
      int moveDown     = GLFW_KEY_LEFT_SHIFT;

      // arrow for looking around
      int lookUp    = GLFW_KEY_UP;
      int lookDown  = GLFW_KEY_DOWN;
      int lookLeft  = GLFW_KEY_LEFT;
      int lookRight = GLFW_KEY_RIGHT;

      // exit key
      int keyExit = GLFW_KEY_ESCAPE;
    };

    void moveInPlaneXZ(float deltaTime, GameObject& gameObject) const;

  private:
    Window&     windowRef;
    KeyMappings mappings{};
    float       moveSpeed = 3.0f; // units per second
    float       lookSpeed = 1.5f; // radians per second
    bool        isKeyPressed(int key) const { return glfwGetKey(windowRef.getGLFWwindow(), key) == GLFW_PRESS; }
  };

} // namespace engine