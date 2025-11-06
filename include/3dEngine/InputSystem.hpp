#pragma once

namespace engine {

  class GameObject;
  class Keyboard;
  class Mouse;

  class InputSystem
  {
  public:
    InputSystem(Keyboard& keyboard, Mouse& mouse);

    void update(float deltaTime, GameObject& cameraObject);

  private:
    Keyboard& keyboard_;
    Mouse&    mouse_;
  };

} // namespace engine
