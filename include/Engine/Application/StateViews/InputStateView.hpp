#pragma once

namespace engine {

class Keyboard;
class Mouse;
class InputSystem;
class ObjectSelectionSystem;
class CameraSystem;

struct InputStateView {
  Keyboard* keyboard = nullptr;
  Mouse* mouse = nullptr;
  InputSystem* inputSystem = nullptr;
  ObjectSelectionSystem* objectSelectionSystem = nullptr;
  CameraSystem* cameraSystem = nullptr;

  /**
   * @brief Check if all required pointers are non-null.
   */
  [[nodiscard]] bool isValid() const {
    return keyboard != nullptr
        && mouse != nullptr
        && inputSystem != nullptr
        && objectSelectionSystem != nullptr
        && cameraSystem != nullptr;
  }
};

}  // namespace engine
