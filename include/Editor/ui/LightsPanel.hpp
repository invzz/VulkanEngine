#ifndef EDITOR_LIGHTSPANEL_HPP
#define EDITOR_LIGHTSPANEL_HPP

#include "Editor/ui/UIPanel.hpp"
#include "Engine/Scene/Scene.hpp"

namespace engine {

class LightsPanel : public UIPanel {
 public:
  LightsPanel(Scene& scene);

  void render(FrameInfo& frameInfo) override;

 private:
  Scene& scene_;
};

}  // namespace engine

#endif  // EDITOR_LIGHTSPANEL_HPP
