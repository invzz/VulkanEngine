#ifndef CUBE_UI_IBLPANEL_HPP
#define CUBE_UI_IBLPANEL_HPP

#include "CubeUI/ui/UIPanel.hpp"
#include "Engine/Scene/Skybox.hpp"
#include "Engine/Systems/IBLSystem.hpp"

namespace engine {
  class IBLPanel : public UIPanel
  {
  public:
    IBLPanel(IBLSystem& iblSystem, Skybox& skybox);
    void render(FrameInfo& frameInfo) override;

  private:
    IBLSystem&          iblSystem_;
    Skybox&             skybox_;
    IBLSystem::Settings settings_;
  };
} // namespace engine

#endif // CUBE_UI_IBLPANEL_HPP
