#pragma once

#include "Engine/Core/Keyboard.hpp"
#include "Engine/Graphics/FrameInfo.hpp"

namespace engine {

  class ObjectSelectionSystem
  {
  public:
    explicit ObjectSelectionSystem(Keyboard& keyboard);

    void update(FrameInfo& frameInfo);

  private:
    Keyboard& keyboard_;

    bool nextKeyWasPressed_   = false;
    bool prevKeyWasPressed_   = false;
    bool cameraKeyWasPressed_ = false;

    bool isKeyPressed(int key) const { return keyboard_.isKeyPressed(key); }
  };

} // namespace engine
