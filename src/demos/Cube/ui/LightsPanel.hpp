#pragma once

#include "3dEngine/GameObject.hpp"
#include "UIPanel.hpp"

namespace engine {

  class LightsPanel : public UIPanel
  {
  public:
    LightsPanel(GameObject::Map& gameObjects);

    void render(FrameInfo& frameInfo) override;

  private:
    GameObject::Map& gameObjects_;
  };

} // namespace engine
