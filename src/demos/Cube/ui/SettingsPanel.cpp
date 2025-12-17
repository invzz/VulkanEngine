#include "SettingsPanel.hpp"

#include <imgui.h>

namespace engine {

  SettingsPanel::SettingsPanel(entt::entity              cameraEntity,
                               Scene*                    scene,
                               IBLSystem&                iblSystem,
                               Skybox&                   skybox,
                               SkyboxSettings&           skySettings,
                               float&                    timeOfDay,
                               PostProcessPushConstants& pushConstants,
                               int&                      debugMode)
      : skySettings_(skySettings), timeOfDay_(timeOfDay)
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
      if (ImGui::CollapsingHeader("Sky"))
      {
        ImGui::Checkbox("Use Procedural Sky", &skySettings_.useProcedural);
        if (skySettings_.useProcedural)
        {
          ImGui::SliderFloat("Time of Day", &timeOfDay_, 0.0f, 6.28318f);
          ImGui::SliderFloat("Rayleigh", &skySettings_.rayleigh, 0.0f, 10.0f);
          ImGui::SliderFloat("Mie", &skySettings_.mie, 0.0f, 1.0f);
          ImGui::SliderFloat("Mie Eccentricity", &skySettings_.mieEccentricity, -1.0f, 1.0f);
        }
      }
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
