#include "Editor/ui/SettingsPanel.hpp"

#include <imgui.h>

#include <memory>

#include "Editor/ui/CameraPanel.hpp"
#include "Editor/ui/DebugPanel.hpp"
#include "Editor/ui/IBLPanel.hpp"
#include "Editor/ui/PostProcessPanel.hpp"
#include "Engine/Graphics/FrameInfo.hpp"
#include "Engine/Scene/Scene.hpp"
#include "Engine/Scene/Skybox.hpp"
#include "Engine/Systems/DustRenderSystem.hpp"
#include "Engine/Systems/IBLSystem.hpp"
#include "Engine/Systems/PostProcessingSystem.hpp"
#include "Engine/Systems/ShadowSystem.hpp"
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
                               ShadowSettings&           shadowSettings,
                               PostProcessPushConstants& pushConstants,
                               bool&                     multithreadedRecordingEnabled,
                               uint32_t&                 multithreadedRecordingThreads,
                               int&                      debugMode)
      : skybox_(skybox), showSkybox_(showSkybox), showGrid_(showGrid), skySettings_(skySettings), dustSettings_(dustSettings), fogSettings_(fogSettings), hzbSettings_(hzbSettings),
        shadowSettings_(shadowSettings), multithreadedRecordingEnabled_(multithreadedRecordingEnabled), multithreadedRecordingThreads_(multithreadedRecordingThreads)
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
      if (ImGui::CollapsingHeader("Shadows (CSM)"))
      {
        ImGui::SliderFloat("Shadow Distance", &shadowSettings_.shadowDistance, 10.0f, 500.0f, "%.0f");
        ImGui::SetItemTooltip("World-space distance covered by shadow cascades. Lower = higher quality.");

        ImGui::SliderFloat("Lambda (Split Distribution)", &shadowSettings_.cascadeLambda, 0.0f, 1.0f, "%.2f");
        ImGui::SetItemTooltip("0 = uniform splits, 1 = logarithmic. Higher values give more detail near camera.");

        ImGui::SliderFloat("Cascade Overlap", &shadowSettings_.cascadeOverlap, 0.0f, 0.5f, "%.2f");
        ImGui::SetItemTooltip("Overlap between cascades for smooth blending. Higher = smoother but more overdraw.");

        ImGui::SliderFloat("Blend Width", &shadowSettings_.cascadeBlendWidth, 0.05f, 0.5f, "%.2f");
        ImGui::SetItemTooltip("Width of blend region at cascade boundaries as fraction of cascade range.");

        ImGui::Checkbox("Debug Visualization", &shadowSettings_.debugVisualization);
        ImGui::SetItemTooltip("Visualize cascade boundaries with colors.");

        // CPU-side conservative culling (opt-in)
        if (ImGui::Checkbox("Enable CPU shadow culling (conservative)", &shadowSettings_.enableShadowCulling))
        {
          ImGui::SetItemTooltip("When enabled, conservatively skip entire cascades/spot/cubemaps on CPU when no shadow casters intersect the light projection.");
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

      if (ImGui::CollapsingHeader("Performance"))
      {
        // Multithreaded recording control (opt-in pilot)
        if (ImGui::Checkbox("Multithreaded recording (secondary CB)", &multithreadedRecordingEnabled_))
        {
          ImGui::SetItemTooltip("When enabled, draw-recording is partitioned across worker threads into secondary command buffers.");
        }

        int tmpThreads = static_cast<int>(multithreadedRecordingThreads_);
        if (ImGui::InputInt("Recording threads (0 = auto)", &tmpThreads))
        {
          if (tmpThreads < 0) tmpThreads = 0;
          multithreadedRecordingThreads_ = static_cast<uint32_t>(tmpThreads);
        }
        ImGui::SetItemTooltip("0 = auto (HW threads - 1); set to 1 to force single-threaded serial recording.");
      }
    }
    ImGui::End();
  }

} // namespace engine
