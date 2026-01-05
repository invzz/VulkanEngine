#ifndef VULKANENGINE_SRC_DEMOS_CUBE_UI_LIGHTSPANEL_HPP
#define VULKANENGINE_SRC_DEMOS_CUBE_UI_LIGHTSPANEL_HPP

#include "Engine/Scene/Scene.hpp"
#include "UIPanel.hpp"

namespace engine {

  class LightsPanel : public UIPanel
  {
  public:
    LightsPanel(Scene& scene);

    void render(FrameInfo& frameInfo) override;

  private:
    Scene& scene_;
  };

} // namespace engine

#endif // VULKANENGINE_SRC_DEMOS_CUBE_UI_LIGHTSPANEL_HPP
