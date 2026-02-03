#ifndef EDITOR_POSTPROCESSPANEL_HPP
#define EDITOR_POSTPROCESSPANEL_HPP

#include "Editor/ui/UIPanel.hpp"
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

#endif // EDITOR_POSTPROCESSPANEL_HPP
