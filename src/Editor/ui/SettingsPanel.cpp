#include "Editor/ui/SettingsPanel.hpp"

#include <algorithm>
#include <imgui.h>
#include <memory>

#include "Engine/Core/ErrorCodes.hpp"
#include "Engine/Core/Logger.hpp"
#include "Engine/EngineState.hpp"
#include "Engine/Graphics/FrameInfo.hpp"
#include "Engine/Graphics/GpuProfiler.hpp"
#include "Engine/Systems/DustRenderSystem.hpp"
#include "Engine/Systems/ShadowSystem.hpp"
#include "Engine/Systems/SkyboxRenderSystem.hpp"

#include "Editor/ui/CameraPanel.hpp"
#include "Editor/ui/DebugPanel.hpp"
#include "Editor/ui/IBLPanel.hpp"
#include "Editor/ui/PostProcessPanel.hpp"
#include "entt/entity/fwd.hpp"

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
        if (!visible_) {
            return;
        }

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

                ImGui::Separator();
                ImGui::Text("Week 6 Cache Metrics");

                if (engineState_->modelRenderSystem != nullptr) {
                    auto const     stats   = engineState_->modelRenderSystem->getMaterialDescriptorCacheStats();
                    uint64_t const total   = stats.cacheHits + stats.cacheMisses;
                    double const   hitRate = (total > 0) ? (100.0 * static_cast<double>(stats.cacheHits) / static_cast<double>(total)) : 0.0;

                    ImGui::Text("Material Descriptor Cache");
                    ImGui::Text("  Hits: %llu  Misses: %llu  Hit Rate: %.1f%%", static_cast<unsigned long long>(stats.cacheHits), static_cast<unsigned long long>(stats.cacheMisses), hitRate);
                    ImGui::Text("  Buffer Writes: %llu", static_cast<unsigned long long>(stats.bufferWrites));
                }

                if (engineState_->resourceManager != nullptr) {
                    auto const     samplerStats = engineState_->resourceManager->getDevice().getSamplerCacheStats();
                    uint64_t const total        = samplerStats.cacheHits + samplerStats.cacheMisses;
                    double const   hitRate      = (total > 0) ? (100.0 * static_cast<double>(samplerStats.cacheHits) / static_cast<double>(total)) : 0.0;

                    ImGui::Text("Sampler Cache");
                    ImGui::Text("  Hits: %llu  Misses: %llu  Hit Rate: %.1f%%", static_cast<unsigned long long>(samplerStats.cacheHits), static_cast<unsigned long long>(samplerStats.cacheMisses), hitRate);
                    ImGui::Text("  Cached Samplers: %llu", static_cast<unsigned long long>(samplerStats.cachedSamplers));
                }

                if (engineState_->modelRenderSystem != nullptr && ImGui::Button("Reset Material Cache Stats")) {
                    engineState_->modelRenderSystem->resetMaterialDescriptorCacheStats();
                }
            }

            if (ImGui::CollapsingHeader("GPU Profiler")) {
                auto& profiler = GpuProfiler::instance();
                bool  enabled  = profiler.isEnabled();
                if (ImGui::Checkbox("Enable Profiling", &enabled)) {
                    profiler.setEnabled(enabled);
                }

                ImGui::Text("Last Frame: %llu", static_cast<unsigned long long>(profiler.lastFrameIndex()));
                ImGui::Text("Frame CPU: %.3f ms", profiler.lastFrameCpuMs());
                if (profiler.lastFrameGpuMs() >= 0.0) {
                    ImGui::Text("Frame GPU (sum pass): %.3f ms", profiler.lastFrameGpuMs());
                } else {
                    ImGui::Text("Frame GPU (sum pass): n/a");
                }
                ImGui::Separator();

                for (const auto& timing : profiler.lastFramePassTimings()) {
                    if (timing.gpuMs >= 0.0) {
                        ImGui::Text("%s  CPU: %.3f ms  GPU: %.3f ms", timing.passName.c_str(), timing.cpuMs, timing.gpuMs);
                    } else {
                        ImGui::Text("%s  CPU: %.3f ms  GPU: n/a", timing.passName.c_str(), timing.cpuMs);
                    }
                }

                if (ImGui::Button("Export Last Frame CSV")) {
                    std::string error;
                    if (!profiler.exportLastFrameCsv("gpu_profile_last_frame.csv", &error)) {
                        Logger::warn(LogChannel::Resource, "Failed to export GPU profiler snapshot: ", error);
                    } else {
                        Logger::info(LogChannel::Resource, "Exported GPU profiler snapshot to gpu_profile_last_frame.csv");
                    }
                }

                ImGui::SameLine();
                if (ImGui::Button("Export Last Frame JSON")) {
                    std::string error;
                    if (!profiler.exportLastFrameJson("gpu_profile_last_frame.json", &error)) {
                        Logger::warn(LogChannel::Resource, "Failed to export GPU profiler JSON snapshot: ", error);
                    } else {
                        Logger::info(LogChannel::Resource, "Exported GPU profiler snapshot to gpu_profile_last_frame.json");
                    }
                }
            }

            if (ImGui::CollapsingHeader("Logging")) {
                int                          minLevel   = static_cast<int>(Logger::minimumLevel());
                static constexpr const char* levelItems = "Error\0Warn\0Info\0Debug\0";
                if (ImGui::Combo("Minimum Level", &minLevel, levelItems)) {
                    minLevel = std::clamp(minLevel, 0, 3);
                    Logger::setMinimumLevel(static_cast<LogLevel>(minLevel));
                }

                bool generalEnabled  = Logger::isChannelEnabled(LogChannel::General);
                bool renderEnabled   = Logger::isChannelEnabled(LogChannel::Render);
                bool syncEnabled     = Logger::isChannelEnabled(LogChannel::Sync);
                bool sceneEnabled    = Logger::isChannelEnabled(LogChannel::Scene);
                bool resourceEnabled = Logger::isChannelEnabled(LogChannel::Resource);

                if (ImGui::Checkbox("General", &generalEnabled)) {
                    Logger::enableChannel(LogChannel::General, generalEnabled);
                }
                ImGui::SameLine();
                if (ImGui::Checkbox("Render", &renderEnabled)) {
                    Logger::enableChannel(LogChannel::Render, renderEnabled);
                }
                ImGui::SameLine();
                if (ImGui::Checkbox("Sync", &syncEnabled)) {
                    Logger::enableChannel(LogChannel::Sync, syncEnabled);
                }

                if (ImGui::Checkbox("Scene", &sceneEnabled)) {
                    Logger::enableChannel(LogChannel::Scene, sceneEnabled);
                }
                ImGui::SameLine();
                if (ImGui::Checkbox("Resource", &resourceEnabled)) {
                    Logger::enableChannel(LogChannel::Resource, resourceEnabled);
                }
            }

            if (ImGui::CollapsingHeader("Error Boundaries (Week 8)")) {
                uint64_t const recoverableCount = ErrorState::countByBoundary(ErrorBoundary::Recoverable);
                uint64_t const fatalCount       = ErrorState::countByBoundary(ErrorBoundary::Fatal);

                ImGui::Text("Recoverable events: %llu", static_cast<unsigned long long>(recoverableCount));
                ImGui::Text("Fatal events: %llu", static_cast<unsigned long long>(fatalCount));

                if (ImGui::Button("Clear Error Events")) {
                    ErrorState::clear();
                }

                auto const recent = ErrorState::recentEvents(12);
                if (recent.empty()) {
                    ImGui::TextDisabled("No error or fallback events recorded.");
                } else {
                    ImGui::Separator();
                    for (const auto& event : recent) {
                        const char* boundaryLabel = (event.boundary == ErrorBoundary::Recoverable) ? "Recoverable" : "Fatal";
                        ImVec4      color         = (event.boundary == ErrorBoundary::Recoverable) ? ImVec4(1.0f, 0.8f, 0.2f, 1.0f) : ImVec4(1.0f, 0.3f, 0.3f, 1.0f);
                        ImGui::TextColored(color, "[%s] code=%u count=%llu", boundaryLabel, static_cast<unsigned>(event.code), static_cast<unsigned long long>(event.count));
                        ImGui::TextWrapped("%s", event.message.c_str());
                    }
                }
            }
        }
        ImGui::End();
    }

}  // namespace engine
