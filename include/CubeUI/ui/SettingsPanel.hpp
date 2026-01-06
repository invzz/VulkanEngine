#ifndef CUBE_UI_SETTINGSPANEL_HPP
#define CUBE_UI_SETTINGSPANEL_HPP

#include <memory>

#include "CubeUI/ui/CameraPanel.hpp"
#include "CubeUI/ui/DebugPanel.hpp"
#include "CubeUI/ui/IBLPanel.hpp"
#include "CubeUI/ui/PostProcessPanel.hpp"
#include "CubeUI/ui/UIPanel.hpp"
#include "Engine/Systems/DustRenderSystem.hpp"
#include "Engine/Systems/SkyboxRenderSystem.hpp"

namespace engine {

  class SettingsPanel : public UIPanel
  {
  public:
    SettingsPanel(entt::entity              cameraEntity,
                  Scene*                    scene,
                  IBLSystem&                iblSystem,
                  Skybox&                   skybox,
                  SkyboxSettings&           skySettings,
                  DustSettings&             dustSettings,
                  FogSettings&              fogSettings,
                  PostProcessPushConstants& pushConstants,
                  int&                      debugMode);

    void               render(FrameInfo& frameInfo) override;
    [[nodiscard]] bool isSeparateWindow() const override { return true; }

  private:
    std::unique_ptr<CameraPanel>      cameraPanel_;
    std::unique_ptr<IBLPanel>         iblPanel_;
    std::unique_ptr<PostProcessPanel> postProcessPanel_;
    std::unique_ptr<DebugPanel>       debugPanel_;

    SkyboxSettings& skySettings_;
    DustSettings&   dustSettings_;
    FogSettings&    fogSettings_;
  };

} // namespace engine

#endif // CUBE_UI_SETTINGSPANEL_HPP
