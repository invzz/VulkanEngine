#pragma once

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
