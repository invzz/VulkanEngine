#ifndef VULKANENGINE_SRC_DEMOS_CUBE_UI_TRANSFORMPANEL_HPP
#define VULKANENGINE_SRC_DEMOS_CUBE_UI_TRANSFORMPANEL_HPP

#include "Engine/Scene/Scene.hpp"
#include "UIPanel.hpp"

namespace engine {

  class TransformPanel : public UIPanel
  {
  public:
    TransformPanel(Scene& scene);

    void render(FrameInfo& frameInfo) override;

  private:
    Scene& scene_;
    bool   lockAxes_ = false;
  };

} // namespace engine

#endif // VULKANENGINE_SRC_DEMOS_CUBE_UI_TRANSFORMPANEL_HPP
