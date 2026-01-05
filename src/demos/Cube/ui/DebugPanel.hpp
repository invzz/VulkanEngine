#ifndef VULKANENGINE_SRC_DEMOS_CUBE_UI_DEBUGPANEL_HPP
#define VULKANENGINE_SRC_DEMOS_CUBE_UI_DEBUGPANEL_HPP

#include "UIPanel.hpp"

namespace engine {
  class DebugPanel : public UIPanel
  {
  public:
    explicit DebugPanel(int& debugMode);
    void render(FrameInfo& frameInfo) override;

  private:
    int& debugMode_;
  };
} // namespace engine

#endif // VULKANENGINE_SRC_DEMOS_CUBE_UI_DEBUGPANEL_HPP
