#ifndef VULKANENGINE_SRC_DEMOS_CUBE_UI_IBLPANEL_HPP
#define VULKANENGINE_SRC_DEMOS_CUBE_UI_IBLPANEL_HPP

#include "Engine/Scene/Skybox.hpp"
#include "Engine/Systems/IBLSystem.hpp"
#include "UIPanel.hpp"

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

#endif // VULKANENGINE_SRC_DEMOS_CUBE_UI_IBLPANEL_HPP
