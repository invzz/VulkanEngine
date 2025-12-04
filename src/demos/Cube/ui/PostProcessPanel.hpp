#pragma once

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
