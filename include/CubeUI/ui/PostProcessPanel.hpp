#ifndef CUBE_UI_POSTPROCESSPANEL_HPP
#define CUBE_UI_POSTPROCESSPANEL_HPP

#include "CubeUI/ui/UIPanel.hpp"
#include "Engine/Systems/PostProcessingSystem.hpp"

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

#endif // CUBE_UI_POSTPROCESSPANEL_HPP
