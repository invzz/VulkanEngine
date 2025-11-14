#include "3dEngine/systems/InputSystem.hpp"

#include "3dEngine/FrameInfo.hpp"
#include "3dEngine/GameObject.hpp"
#include "3dEngine/Keyboard.hpp"
#include "3dEngine/Mouse.hpp"

namespace engine {

  InputSystem::InputSystem(Keyboard& keyboard, Mouse& mouse) : keyboard_{keyboard}, mouse_{mouse} {}

  void InputSystem::update(FrameInfo& frameInfo, GameObject& cameraObject)
  {
    // Control the selected object (camera or a game object)
    GameObject* controllableObject = frameInfo.selectedObject ? frameInfo.selectedObject : &cameraObject;

    keyboard_.moveInPlaneXZ(frameInfo.frameTime, *controllableObject);
    mouse_.lookAround(frameInfo.frameTime, *controllableObject);
  }

} // namespace engine
