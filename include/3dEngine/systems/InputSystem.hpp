#pragma once

#include "../FrameInfo.hpp"
#include "../GameObject.hpp"
#include "../Keyboard.hpp"
#include "../Mouse.hpp"
#include "../Window.hpp"

namespace engine {

  class InputSystem
  {
  public:
    InputSystem(Keyboard& keyboard, Mouse& mouse, Window& window);

    void update(FrameInfo& frameInfo);

  private:
    Keyboard& keyboard_;
    Mouse&    mouse_;
    Window&   window_;

    // Toggle cursor state tracking
    bool lastToggleKeyState_ = false;
  };

} // namespace engine
