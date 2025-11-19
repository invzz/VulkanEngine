#pragma once

#include "3dEngine/GameObject.hpp"
#include "UIPanel.hpp"

namespace engine {

  /**
   * @brief Panel for animation controls
   */
  class AnimationPanel : public UIPanel
  {
  public:
    explicit AnimationPanel(GameObject::Map& gameObjects);

    void render(FrameInfo& frameInfo) override;

  private:
    GameObject::Map& gameObjects_;
  };

} // namespace engine
