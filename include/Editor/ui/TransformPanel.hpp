#ifndef EDITOR_TRANSFORMPANEL_HPP
#define EDITOR_TRANSFORMPANEL_HPP

#include "Editor/ui/UIPanel.hpp"
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

#endif // EDITOR_TRANSFORMPANEL_HPP
