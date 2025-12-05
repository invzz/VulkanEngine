#pragma once

#include "Engine/Scene/Scene.hpp"
#include "UIPanel.hpp"

namespace engine {

  class LightsPanel : public UIPanel
  {
  public:
    LightsPanel(Scene& scene);

    void render(FrameInfo& frameInfo) override;

  private:
    Scene& scene_;
  };

} // namespace engine
