#include "3dEngine/systems/InputSystem.hpp"

#include "3dEngine/FrameInfo.hpp"
#include "3dEngine/GameObject.hpp"
#include "3dEngine/Keyboard.hpp"
#include "3dEngine/Mouse.hpp"
#include "3dEngine/Window.hpp"

namespace engine {

  InputSystem::InputSystem(Keyboard& keyboard, Mouse& mouse, Window& window) : keyboard_{keyboard}, mouse_{mouse}, window_{window} {}

  void InputSystem::update(FrameInfo& frameInfo)
  {
    // Handle cursor toggle (ESC key with debouncing)
    bool toggleKeyPressed = keyboard_.isKeyPressed(keyboard_.mappings.toggleCursor);
    if (toggleKeyPressed && !lastToggleKeyState_)
    {
      window_.toggleCursor();
    }
    lastToggleKeyState_ = toggleKeyPressed;

    // Only process movement/look input when in FPS mode (cursor hidden)
    // When cursor is visible, user should only interact with UI
    if (window_.isCursorVisible())
    {
      return;
    }

    // Control the selected object (camera or a game object)
    GameObject* controllableObject = frameInfo.selectedObject ? frameInfo.selectedObject : &frameInfo.cameraObject;

    keyboard_.moveInPlaneXZ(frameInfo.frameTime, *controllableObject);
    mouse_.lookAround(frameInfo.frameTime, *controllableObject);
  }

} // namespace engine
