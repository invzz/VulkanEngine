#pragma once

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
