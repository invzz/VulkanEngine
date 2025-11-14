#pragma once

#include "../FrameInfo.hpp"
#include "../GameObject.hpp"
#include "../Keyboard.hpp"
#include "../Mouse.hpp"

namespace engine {

  class InputSystem
  {
  public:
    InputSystem(Keyboard& keyboard, Mouse& mouse);

    void update(FrameInfo& frameInfo, GameObject& cameraObject);

  private:
    Keyboard& keyboard_;
    Mouse&    mouse_;
  };

} // namespace engine
