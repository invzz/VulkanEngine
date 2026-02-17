#include "Editor/ui/SettingsPanel.hpp"

#include <algorithm>
#include <imgui.h>
#include <imgui_impl_vulkan.h>
#include <memory>

#include "Engine/EngineState.hpp"
#include "Engine/Graphics/FrameInfo.hpp"
#include "Engine/Systems/DustRenderSystem.hpp"
#include "Engine/Systems/ShadowSystem.hpp"
#include "Engine/Systems/SkyboxRenderSystem.hpp"

#include "Editor/ui/CameraPanel.hpp"
#include "Editor/ui/DebugPanel.hpp"
#include "Editor/ui/IBLPanel.hpp"
#include "Editor/ui/PostProcessPanel.hpp"
#include "entt/entity/fwd.hpp"

// Cache ImGui texture IDs for shadow-map thumbnails (one per directional cascade)
static ImTextureID s_csmShadowImTex[engine::ShadowSystem::DIRECTIONAL_CASCADE_COUNT]         = {};
static VkImageView s_csmShadowLastImageView[engine::ShadowSystem::DIRECTIONAL_CASCADE_COUNT] = {VK_NULL_HANDLE};

namespace engine {

    SettingsPanel::SettingsPanel(EngineState* engineState, bool& multithreadedRecordingEnabled, uint32_t& multithreadedRecordingThreads, int& debugMode)
        : engineState_(engineState), multithreadedRecordingEnabled_(multithreadedRecordingEnabled), multithreadedRecordingThreads_(multithreadedRecordingThreads) {
        // CameraPanel still expects entt::entity + Scene*
        entt::entity camEntity = ((engineState_ != nullptr) ? engineState_->cameraEntity : entt::null);
        cameraPanel_           = std::make_unique<CameraPanel>(camEntity, &engineState_->scene);
        iblPanel_              = std::make_unique<IBLPanel>(engineState_);
        postProcessPanel_      = std::make_unique<PostProcessPanel>(engineState_->postProcessPush);
        debugPanel_            = std::make_unique<DebugPanel>(debugMode);
    }

    void SettingsPanel::render(FrameInfo& frameInfo) {
        if (!visible_)
            return;

        if (ImGui::Begin("Settings", &visible_)) {
            ImGui::Checkbox("Show Skybox", &engineState_->showSkybox);
            ImGui::SameLine();
            ImGui::Checkbox("Show Grid", &engineState_->showGrid);
            if (engineState_->showSkybox && engineState_->skybox == nullptr) {
                ImGui::TextDisabled("(Skybox will load next frame)");
            }
            ImGui::Separator();

            if (ImGui::CollapsingHeader("Sky")) {
                ImGui::Checkbox("Debug Cubemap Faces", &engineState_->skySettings.debugCubemapFaces);
            }
            if (ImGui::CollapsingHeader("Shadows (CSM)")) {
                ImGui::SliderFloat("Shadow Distance", &engineState_->shadowSettings.shadowDistance, 10.0f, 500.0f, "%.0f");
                ImGui::SetItemTooltip("World-space distance covered by shadow cascades. Lower = higher quality.");

                ImGui::SliderFloat("Lambda (Split Distribution)", &engineState_->shadowSettings.cascadeLambda, 0.0f, 1.0f, "%.2f");
                ImGui::SetItemTooltip("0 = uniform splits, 1 = logarithmic. Higher values give more detail near camera.");

                ImGui::SliderFloat("Cascade Overlap", &engineState_->shadowSettings.cascadeOverlap, 0.0f, 0.5f, "%.2f");
                ImGui::SetItemTooltip("Overlap between cascades for smooth blending. Higher = smoother but more overdraw.");

                ImGui::SliderFloat("Blend Width", &engineState_->shadowSettings.cascadeBlendWidth, 0.05f, 0.5f, "%.2f");
                ImGui::SetItemTooltip("Width of blend region at cascade boundaries as fraction of cascade range.");

                ImGui::Checkbox("Debug Visualization", &engineState_->shadowSettings.debugVisualization);
                ImGui::SetItemTooltip("Visualize cascade boundaries with colors.");

                // CPU-side conservative culling (opt-in)
                if (ImGui::Checkbox("Enable CPU shadow culling (conservative)", &engineState_->shadowSettings.enableShadowCulling)) {
                    ImGui::SetItemTooltip("When enabled, conservatively skip entire cascades/spot/cubemaps on CPU when no shadow casters intersect the light projection.");
                }
            }
            if (ImGui::CollapsingHeader("Fog")) {
                ImGui::SliderFloat("Density", &engineState_->fogSettings.density, 0.0f, 0.1f, "%.4f");
                ImGui::SliderFloat("Height", &engineState_->fogSettings.height, -100.0f, 100.0f);
                ImGui::SliderFloat("Height Density", &engineState_->fogSettings.heightDensity, 0.0f, 1.0f);
                ImGui::Checkbox("Use Sky Color", &engineState_->fogSettings.useSkyColor);
                if (!engineState_->fogSettings.useSkyColor) {
                    ImGui::ColorEdit3("Fog Color", &engineState_->fogSettings.color.x);
                }

                ImGui::Separator();
                ImGui::Text("God Rays");
                ImGui::Checkbox("Enable God Rays", &engineState_->fogSettings.enableGodRays);
                if (engineState_->fogSettings.enableGodRays) {
                    ImGui::SliderFloat("GR Density", &engineState_->fogSettings.godRayDensity, 0.0f, 2.0f);
                    ImGui::SliderFloat("GR Weight", &engineState_->fogSettings.godRayWeight, 0.0f, 0.1f, "%.4f");
                    ImGui::SliderFloat("GR Decay", &engineState_->fogSettings.godRayDecay, 0.8f, 1.0f);
                    ImGui::SliderFloat("GR Exposure", &engineState_->fogSettings.godRayExposure, 0.0f, 2.0f);
                }
            }
            if (ImGui::CollapsingHeader("Dust")) {
                ImGui::Checkbox("Enable Dust", &engineState_->dustSettings.enabled);
                if (engineState_->dustSettings.enabled) {
                    ImGui::SliderFloat("Size", &engineState_->dustSettings.particleSize, 1.0f, 50.0f);
                    ImGui::SliderFloat("Alpha", &engineState_->dustSettings.alpha, 0.0f, 1.0f);
                    ImGui::SliderFloat("Box Size", &engineState_->dustSettings.boxSize, 10.0f, 100.0f);
                    ImGui::SliderFloat("Height Falloff", &engineState_->dustSettings.heightFalloff, 0.0f, 1.0f);
                    ImGui::SliderInt("Count", &engineState_->dustSettings.particleCount, 0, 10000);
                }
            }
            if (ImGui::CollapsingHeader("Camera")) {
                cameraPanel_->render(frameInfo);
            }
            if (ImGui::CollapsingHeader("Environment (IBL)")) {
                iblPanel_->render(frameInfo);
            }
            if (ImGui::CollapsingHeader("Post Processing")) {
                postProcessPanel_->render(frameInfo);
            }
            if (ImGui::CollapsingHeader("Occlusion Culling (HZB)")) {
                bool enabled = (engineState_->hzbSettings.enabled != 0);
                if (ImGui::Checkbox("Enable HZB", &enabled)) {
                    engineState_->hzbSettings.enabled = enabled ? 1 : 0;
                }

                if (engineState_->hzbSettings.enabled != 0) {
                    ImGui::SliderInt("Max Mip Level", &engineState_->hzbSettings.maxMipLevel, 1, 12);
                    ImGui::SetItemTooltip("Higher = coarser testing. Limits the maximum mip level used.");

                    ImGui::SliderFloat("Min Screen Pixels", &engineState_->hzbSettings.minScreenPixels, 0.5f, 32.0f, "%.1f");
                    ImGui::SetItemTooltip("Objects smaller than this skip HZB (early-z handles them).");

                    ImGui::SliderFloat("Mip Scale", &engineState_->hzbSettings.screenSizeScale, 0.5f, 2.0f, "%.2f");
                    ImGui::SetItemTooltip("Bias for mip selection. Higher = coarser mips = faster but more false positives.");
                }
            }
            if (ImGui::CollapsingHeader("Debug")) {
                debugPanel_->render(frameInfo);

                // Extra CSM diagnostics when a CSM debug view is active
                if (frameInfo.debugMode >= 15 && frameInfo.debugMode <= 19) {
                    ImGui::Separator();
                    ImGui::Text("CSM diagnostics:");
                    int cascadeCount = engineState_->shadowSystem->getDirectionalCascadeCount();
                    ImGui::Text("Directional cascades rendered: %d / %d", cascadeCount, engine::ShadowSystem::DIRECTIONAL_CASCADE_COUNT);
                    ImGui::Text("Shadow light count: %d", engineState_->shadowSystem->getShadowLightCount());
                    glm::vec4 splits = engineState_->shadowSystem->getDirectionalCascadeSplits();
                    ImGui::Text("Splits (m): %.2f, %.2f, %.2f, %.2f", splits.x, splits.y, splits.z, splits.w);
                    ImGui::Text("Cascade base index: %d", engineState_->shadowSystem->getDirectionalCascadeBaseIndex());

                    // Per-cascade textual diagnostics
                    for (int i = 0; i < engine::ShadowSystem::DIRECTIONAL_CASCADE_COUNT; ++i) {
                        float texel = engineState_->shadowSystem->getDirectionalCascadeWorldUnitsPerTexel(i);
                        ImGui::Text(" Cascade %d: %s  (worldUnits/texel = %.4f)", i, (i < cascadeCount) ? "rendered" : "skipped", texel);
                    }

                    // Show shadow-map thumbnails for visual verification
                    ImGui::Spacing();
                    ImGui::Text("Shadow map previews:");
                    ImGui::BeginGroup();
                    for (int i = 0; i < engine::ShadowSystem::DIRECTIONAL_CASCADE_COUNT; ++i) {
                        int                   mapIndex = engineState_->shadowSystem->getDirectionalCascadeBaseIndex() + i;
                        VkDescriptorImageInfo info     = engineState_->shadowSystem->getShadowMapDescriptorInfo(mapIndex);

                        // Cache ImGui texture id per image view
                        if (s_csmShadowLastImageView[i] != info.imageView || !s_csmShadowImTex[i]) {
                            s_csmShadowImTex[i]         = (ImTextureID) ImGui_ImplVulkan_AddTexture(info.sampler, info.imageView, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL);
                            s_csmShadowLastImageView[i] = info.imageView;
                        }

                        ImGui::BeginGroup();
                        ImGui::Text("Cascade %d", i);
                        ImGui::Image(s_csmShadowImTex[i], ImVec2(128.0f, 128.0f));
                        ImGui::TextDisabled("%s", (i < cascadeCount) ? "rendered" : "skipped");
                        ImGui::EndGroup();

                        if (i + 1 < engine::ShadowSystem::DIRECTIONAL_CASCADE_COUNT)
                            ImGui::SameLine();
                    }
                    ImGui::EndGroup();
                }
            }

            if (ImGui::CollapsingHeader("Performance")) {
                // Multithreaded recording control (opt-in pilot)
                if (ImGui::Checkbox("Multithreaded recording (secondary CB)", &multithreadedRecordingEnabled_)) {
                    ImGui::SetItemTooltip("When enabled, draw-recording is partitioned across worker threads into secondary command buffers.");
                }

                int tmpThreads = static_cast<int>(multithreadedRecordingThreads_);
                if (ImGui::InputInt("Recording threads (0 = auto)", &tmpThreads)) {
                    tmpThreads                     = std::max(tmpThreads, 0);
                    multithreadedRecordingThreads_ = static_cast<uint32_t>(tmpThreads);
                }
                ImGui::SetItemTooltip("0 = auto (HW threads - 1); set to 1 to force single-threaded serial recording.");
            }
        }
        ImGui::End();
    }

}  // namespace engine
