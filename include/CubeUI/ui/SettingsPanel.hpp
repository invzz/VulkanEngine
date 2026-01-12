#ifndef CUBE_UI_SETTINGSPANEL_HPP
#define CUBE_UI_SETTINGSPANEL_HPP

#include <memory>

#include "CubeUI/ui/CameraPanel.hpp"
#include "CubeUI/ui/DebugPanel.hpp"
#include "CubeUI/ui/IBLPanel.hpp"
#include "CubeUI/ui/PostProcessPanel.hpp"
#include "CubeUI/ui/UIPanel.hpp"
#include "Engine/Graphics/FrameInfo.hpp"
#include "Engine/Systems/DustRenderSystem.hpp"
#include "Engine/Systems/SkyboxRenderSystem.hpp"

namespace engine {

  class SettingsPanel : public UIPanel
  {
  public:
    SettingsPanel(entt::entity              cameraEntity,
                  Scene*                    scene,
                  IBLSystem&                iblSystem,
                  std::unique_ptr<Skybox>*  skybox,
                  bool&                     showSkybox,
                  bool&                     showGrid,
                  SkyboxSettings&           skySettings,
                  DustSettings&             dustSettings,
                  FogSettings&              fogSettings,
                  HZBSettings&              hzbSettings,
                  PostProcessPushConstants& pushConstants,
                  int&                      debugMode,
                  bool&                     showBakedRaw);
    void render(FrameInfo& frameInfo) override;

    std::unique_ptr<Skybox>* skybox_;
    bool&                    showSkybox_;
    bool&                    showGrid_;

    SkyboxSettings& skySettings_;
    DustSettings&   dustSettings_;
    FogSettings&    fogSettings_;
    HZBSettings&    hzbSettings_;

    std::unique_ptr<CameraPanel>      cameraPanel_;
    std::unique_ptr<IBLPanel>         iblPanel_;
    std::unique_ptr<PostProcessPanel> postProcessPanel_;
    std::unique_ptr<DebugPanel>       debugPanel_;
    bool&                             showBakedRaw_;
  };

} // namespace engine

#endif // CUBE_UI_SETTINGSPANEL_HPP
