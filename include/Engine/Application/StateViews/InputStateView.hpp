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
};

}  // namespace engine
