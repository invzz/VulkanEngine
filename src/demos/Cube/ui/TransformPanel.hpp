#pragma once

#include "3dEngine/GameObject.hpp"
#include "UIPanel.hpp"

namespace engine {

  class TransformPanel : public UIPanel
  {
  public:
    TransformPanel(GameObject::Map& gameObjects);

    void render(FrameInfo& frameInfo) override;

  private:
    GameObject::Map& gameObjects_;
    bool             lockAxes_ = false;
  };

} // namespace engine
