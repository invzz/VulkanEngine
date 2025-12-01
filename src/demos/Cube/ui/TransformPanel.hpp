#pragma once

#include "3dEngine/GameObject.hpp"
#include "UIPanel.hpp"

namespace engine {

  class TransformPanel : public UIPanel
  {
  public:
    TransformPanel(GameObjectManager& objectManager);

    void render(FrameInfo& frameInfo) override;

  private:
    GameObjectManager& objectManager_;
    bool               lockAxes_ = false;
  };

} // namespace engine
