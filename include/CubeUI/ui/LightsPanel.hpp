#ifndef CUBE_UI_LIGHTSPANEL_HPP
#define CUBE_UI_LIGHTSPANEL_HPP

#include "CubeUI/ui/UIPanel.hpp"
#include "Engine/Scene/Scene.hpp"

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

#endif // CUBE_UI_LIGHTSPANEL_HPP
