#ifndef EDITOR_SETTINGSPANEL_HPP
#define EDITOR_SETTINGSPANEL_HPP

#include <memory>

#include "Editor/ui/CameraPanel.hpp"
#include "Editor/ui/DebugPanel.hpp"
#include "Editor/ui/IBLPanel.hpp"
#include "Editor/ui/PostProcessPanel.hpp"
#include "Editor/ui/UIPanel.hpp"
#include "Engine/Graphics/FrameInfo.hpp"
#include "Engine/Systems/DustRenderSystem.hpp"
#include "Engine/Systems/ShadowSystem.hpp"
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
                  ShadowSettings&           shadowSettings,
                  PostProcessPushConstants& pushConstants,
                  bool&                     multithreadedRecordingEnabled,
                  uint32_t&                 multithreadedRecordingThreads,
                  int&                      debugMode);
    void render(FrameInfo& frameInfo) override;

    std::unique_ptr<Skybox>* skybox_;
    bool&                    showSkybox_;
    bool&                    showGrid_;

    SkyboxSettings& skySettings_;
    DustSettings&   dustSettings_;
    FogSettings&    fogSettings_;
    HZBSettings&    hzbSettings_;
    ShadowSettings& shadowSettings_;

    std::unique_ptr<CameraPanel>      cameraPanel_;
    std::unique_ptr<IBLPanel>         iblPanel_;
    std::unique_ptr<PostProcessPanel> postProcessPanel_;
    std::unique_ptr<DebugPanel>       debugPanel_;

    // Demo UI control for multithreaded recording (references owned by App)
    bool&     multithreadedRecordingEnabled_;
    uint32_t& multithreadedRecordingThreads_;
  };

} // namespace engine

#endif // EDITOR_SETTINGSPANEL_HPP
