#include "SettingsPanel.hpp"

#include <imgui.h>

namespace engine {

  SettingsPanel::SettingsPanel(entt::entity              cameraEntity,
                               Scene*                    scene,
                               IBLSystem&                iblSystem,
                               Skybox&                   skybox,
                               SkyboxSettings&           skySettings,
                               DustSettings&             dustSettings,
                               FogSettings&              fogSettings,
                               float&                    timeOfDay,
                               PostProcessPushConstants& pushConstants,
                               int&                      debugMode)
      : skySettings_(skySettings), dustSettings_(dustSettings), fogSettings_(fogSettings), timeOfDay_(timeOfDay)
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
      if (ImGui::CollapsingHeader("Fog"))
      {
        ImGui::SliderFloat("Density", &fogSettings_.density, 0.0f, 0.1f, "%.4f");
        ImGui::SliderFloat("Height", &fogSettings_.height, -100.0f, 100.0f);
        ImGui::SliderFloat("Height Density", &fogSettings_.heightDensity, 0.0f, 1.0f);
        ImGui::Checkbox("Use Sky Color", &fogSettings_.useSkyColor);
        if (!fogSettings_.useSkyColor)
        {
          ImGui::ColorEdit3("Fog Color", &fogSettings_.color.x);
        }

        ImGui::Separator();
        ImGui::Text("God Rays");
        ImGui::Checkbox("Enable God Rays", &fogSettings_.enableGodRays);
        if (fogSettings_.enableGodRays)
        {
          ImGui::SliderFloat("GR Density", &fogSettings_.godRayDensity, 0.0f, 2.0f);
          ImGui::SliderFloat("GR Weight", &fogSettings_.godRayWeight, 0.0f, 0.1f, "%.4f");
          ImGui::SliderFloat("GR Decay", &fogSettings_.godRayDecay, 0.8f, 1.0f);
          ImGui::SliderFloat("GR Exposure", &fogSettings_.godRayExposure, 0.0f, 2.0f);
        }
      }
      if (ImGui::CollapsingHeader("Dust"))
      {
        ImGui::Checkbox("Enable Dust", &dustSettings_.enabled);
        if (dustSettings_.enabled)
        {
          ImGui::SliderFloat("Size", &dustSettings_.particleSize, 1.0f, 50.0f);
          ImGui::SliderFloat("Alpha", &dustSettings_.alpha, 0.0f, 1.0f);
          ImGui::SliderFloat("Box Size", &dustSettings_.boxSize, 10.0f, 100.0f);
          ImGui::SliderFloat("Height Falloff", &dustSettings_.heightFalloff, 0.0f, 1.0f);
          ImGui::SliderInt("Count", &dustSettings_.particleCount, 0, 10000);
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
