#ifndef VULKANENGINE_SRC_DEMOS_CUBE_UI_POSTPROCESSPANEL_HPP
#define VULKANENGINE_SRC_DEMOS_CUBE_UI_POSTPROCESSPANEL_HPP

#include "Engine/Systems/PostProcessingSystem.hpp"
#include "UIPanel.hpp"

namespace engine {
  class PostProcessPanel : public UIPanel
  {
  public:
    PostProcessPanel(PostProcessPushConstants& pushConstants);
    void render(FrameInfo& frameInfo) override;

  private:
    PostProcessPushConstants& pushConstants;
  };
} // namespace engine

#endif // VULKANENGINE_SRC_DEMOS_CUBE_UI_POSTPROCESSPANEL_HPP
