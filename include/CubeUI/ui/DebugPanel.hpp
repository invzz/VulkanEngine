#ifndef CUBE_UI_DEBUGPANEL_HPP
#define CUBE_UI_DEBUGPANEL_HPP

#include "CubeUI/ui/UIPanel.hpp"

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

#endif // CUBE_UI_DEBUGPANEL_HPP
