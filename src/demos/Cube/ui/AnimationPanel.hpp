#ifndef VULKANENGINE_SRC_DEMOS_CUBE_UI_ANIMATIONPANEL_HPP
#define VULKANENGINE_SRC_DEMOS_CUBE_UI_ANIMATIONPANEL_HPP

#include "Engine/Scene/Scene.hpp"
#include "UIPanel.hpp"

namespace engine {

  /**
   * @brief Panel for animation controls
   */
  class AnimationPanel : public UIPanel
  {
  public:
    explicit AnimationPanel(Scene& scene);

    void render(FrameInfo& frameInfo) override;

  private:
    Scene& scene_;
  };

} // namespace engine

#endif // VULKANENGINE_SRC_DEMOS_CUBE_UI_ANIMATIONPANEL_HPP
