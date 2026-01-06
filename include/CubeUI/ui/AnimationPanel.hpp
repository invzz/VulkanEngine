#ifndef CUBE_UI_ANIMATIONPANEL_HPP
#define CUBE_UI_ANIMATIONPANEL_HPP

#include "CubeUI/ui/UIPanel.hpp"
#include "Engine/Scene/Scene.hpp"

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

#endif // CUBE_UI_ANIMATIONPANEL_HPP
