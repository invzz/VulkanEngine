#pragma once

#include "Engine/Core/Keyboard.hpp"
#include "Engine/Core/Mouse.hpp"
#include "Engine/Core/Window.hpp"
#include "Engine/Graphics/FrameInfo.hpp"

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
