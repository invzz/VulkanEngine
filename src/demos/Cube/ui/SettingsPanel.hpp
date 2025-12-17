#pragma once

#include <memory>

#include "CameraPanel.hpp"
#include "DebugPanel.hpp"
#include "Engine/Systems/SkyboxRenderSystem.hpp"
#include "IBLPanel.hpp"
#include "PostProcessPanel.hpp"
#include "UIPanel.hpp"

namespace engine {

  class SettingsPanel : public UIPanel
  {
  public:
    SettingsPanel(entt::entity              cameraEntity,
                  Scene*                    scene,
                  IBLSystem&                iblSystem,
                  Skybox&                   skybox,
                  SkyboxSettings&           skySettings,
                  float&                    timeOfDay,
                  PostProcessPushConstants& pushConstants,
                  int&                      debugMode);

    void render(FrameInfo& frameInfo) override;
    bool isSeparateWindow() const override { return true; }

  private:
    std::unique_ptr<CameraPanel>      cameraPanel_;
    std::unique_ptr<IBLPanel>         iblPanel_;
    std::unique_ptr<PostProcessPanel> postProcessPanel_;
    std::unique_ptr<DebugPanel>       debugPanel_;

    SkyboxSettings& skySettings_;
    float&          timeOfDay_;
  };

} // namespace engine
