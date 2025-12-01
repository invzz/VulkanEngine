#pragma once

#include "3dEngine/GameObject.hpp"
#include "3dEngine/GameObjectManager.hpp"
#include "UIPanel.hpp"

namespace engine {

  /**
   * @brief Panel for animation controls
   */
  class AnimationPanel : public UIPanel
  {
  public:
    explicit AnimationPanel(GameObjectManager& objectManager);

    void render(FrameInfo& frameInfo) override;

  private:
    GameObjectManager& objectManager_;
  };

} // namespace engine
