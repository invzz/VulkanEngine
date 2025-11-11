#include "3dEngine/systems/InputSystem.hpp"

#include "3dEngine/GameObject.hpp"
#include "3dEngine/Keyboard.hpp"
#include "3dEngine/Mouse.hpp"

namespace engine {

  InputSystem::InputSystem(Keyboard& keyboard, Mouse& mouse) : keyboard_{keyboard}, mouse_{mouse} {}

  void InputSystem::update(float deltaTime, GameObject& controllableObject)
  {
    keyboard_.moveInPlaneXZ(deltaTime, controllableObject);
    mouse_.lookAround(deltaTime, controllableObject);
  }

} // namespace engine
