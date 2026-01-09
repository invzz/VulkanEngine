#ifndef CUBE_UI_TRANSFORMPANEL_HPP
#define CUBE_UI_TRANSFORMPANEL_HPP

#include "CubeUI/ui/UIPanel.hpp"
#include "Engine/Scene/Scene.hpp"

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

#endif // CUBE_UI_TRANSFORMPANEL_HPP
