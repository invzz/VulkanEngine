#pragma once

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
