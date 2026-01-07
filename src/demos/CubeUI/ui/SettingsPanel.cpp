#include "CubeUI/ui/SettingsPanel.hpp"

#include <imgui.h>

#include <memory>

#include "CubeUI/ui/CameraPanel.hpp"
#include "CubeUI/ui/DebugPanel.hpp"
#include "CubeUI/ui/IBLPanel.hpp"
#include "CubeUI/ui/PostProcessPanel.hpp"
#include "Engine/Graphics/FrameInfo.hpp"
#include "Engine/Scene/Scene.hpp"
#include "Engine/Scene/Skybox.hpp"
#include "Engine/Systems/DustRenderSystem.hpp"
#include "Engine/Systems/IBLSystem.hpp"
#include "Engine/Systems/PostProcessingSystem.hpp"
#include "Engine/Systems/SkyboxRenderSystem.hpp"
#include "entt/entity/fwd.hpp"

namespace engine {

  SettingsPanel::SettingsPanel(entt::entity              cameraEntity,
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
                               int&                      debugMode)
      : skybox_(skybox), showSkybox_(showSkybox), showGrid_(showGrid), skySettings_(skySettings), dustSettings_(dustSettings), fogSettings_(fogSettings), hzbSettings_(hzbSettings)
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
      ImGui::Checkbox("Show Skybox", &showSkybox_);
      ImGui::SameLine();
      ImGui::Checkbox("Show Grid", &showGrid_);
      if (showSkybox_ && (skybox_ != nullptr) && (*skybox_ == nullptr))
      {
        ImGui::TextDisabled("(Skybox will load next frame)");
      }
      ImGui::Separator();

      if (ImGui::CollapsingHeader("Sky"))
      {
        ImGui::Checkbox("Debug Cubemap Faces", &skySettings_.debugCubemapFaces);
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
      if (ImGui::CollapsingHeader("Occlusion Culling (HZB)"))
      {
        bool enabled = (hzbSettings_.enabled != 0);
        if (ImGui::Checkbox("Enable HZB", &enabled))
        {
          hzbSettings_.enabled = enabled ? 1 : 0;
        }

        if (hzbSettings_.enabled)
        {
          ImGui::SliderInt("Max Mip Level", &hzbSettings_.maxMipLevel, 1, 12);
          ImGui::SetItemTooltip("Higher = coarser testing. Limits the maximum mip level used.");

          ImGui::SliderFloat("Min Screen Pixels", &hzbSettings_.minScreenPixels, 0.5f, 32.0f, "%.1f");
          ImGui::SetItemTooltip("Objects smaller than this skip HZB (early-z handles them).");

          ImGui::SliderFloat("Mip Scale", &hzbSettings_.screenSizeScale, 0.5f, 2.0f, "%.2f");
          ImGui::SetItemTooltip("Bias for mip selection. Higher = coarser mips = faster but more false positives.");
        }
      }
      if (ImGui::CollapsingHeader("Debug"))
      {
        debugPanel_->render(frameInfo);
      }
    }
    ImGui::End();
  }

} // namespace engine
