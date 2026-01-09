#ifndef CUBE_UI_INSPECTORPANEL_HPP
#define CUBE_UI_INSPECTORPANEL_HPP

#include <memory>

#include "CubeUI/ui/AnimationPanel.hpp"
#include "CubeUI/ui/LightsPanel.hpp"
#include "CubeUI/ui/TransformPanel.hpp"
#include "CubeUI/ui/UIPanel.hpp"

namespace engine {

  class InspectorPanel : public UIPanel
  {
  public:
    InspectorPanel(Scene& scene);

    void               render(FrameInfo& frameInfo) override;
    [[nodiscard]] bool isSeparateWindow() const override { return true; }

  private:
    std::unique_ptr<TransformPanel> transformPanel_;
    std::unique_ptr<LightsPanel>    lightsPanel_;
    std::unique_ptr<AnimationPanel> animationPanel_;
  };

} // namespace engine

#endif // CUBE_UI_INSPECTORPANEL_HPP
