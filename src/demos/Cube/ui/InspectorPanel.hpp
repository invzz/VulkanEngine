#pragma once

#include <memory>

#include "AnimationPanel.hpp"
#include "LightsPanel.hpp"
#include "TransformPanel.hpp"
#include "UIPanel.hpp"

namespace engine {

  class InspectorPanel : public UIPanel
  {
  public:
    InspectorPanel(Scene& scene);

    void render(FrameInfo& frameInfo) override;
    bool isSeparateWindow() const override { return true; }

  private:
    std::unique_ptr<TransformPanel> transformPanel_;
    std::unique_ptr<LightsPanel>    lightsPanel_;
    std::unique_ptr<AnimationPanel> animationPanel_;
  };

} // namespace engine
