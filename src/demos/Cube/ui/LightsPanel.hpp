#pragma once

#include "Engine/Scene/GameObject.hpp"
#include "UIPanel.hpp"

namespace engine {

  class LightsPanel : public UIPanel
  {
  public:
    LightsPanel(GameObjectManager& objectManager);

    void render(FrameInfo& frameInfo) override;

  private:
    GameObjectManager& objectManager_;
  };

} // namespace engine
