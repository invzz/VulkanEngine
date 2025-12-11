#include "SettingsPanel.hpp"

#include <imgui.h>

namespace engine {

  SettingsPanel::SettingsPanel(entt::entity              cameraEntity,
                               Scene*                    scene,
                               IBLSystem&                iblSystem,
                               Skybox&                   skybox,
                               PostProcessPushConstants& pushConstants,
                               int&                      debugMode)
  {
    cameraPanel_      = std::make_unique<CameraPanel>(cameraEntity, scene);
    iblPanel_         = std::make_unique<IBLPanel>(iblSystem, skybox);
    postProcessPanel_ = std::make_unique<PostProcessPanel>(pushConstants);
    debugPanel_       = std::make_unique<DebugPanel>(debugMode);
  }

  void SettingsPanel::render(FrameInfo& frameInfo)
  {
    if (!visible_) return;

    if (ImGui::Begin("Settings", &visible_))
    {
      if (ImGui::CollapsingHeader("Camera"))
      {
        cameraPanel_->render(frameInfo);
      }
      if (ImGui::CollapsingHeader("Environment (IBL)"))
      {
        iblPanel_->render(frameInfo);
      }
      if (ImGui::CollapsingHeader("Post Processing"))
      {
        postProcessPanel_->render(frameInfo);
      }
      if (ImGui::CollapsingHeader("Debug"))
      {
        debugPanel_->render(frameInfo);
      }
    }
    ImGui::End();
  }

} // namespace engine
