#include "Editor/ui/SettingsPanel.hpp"

#include <algorithm>
#include <imgui.h>
#include <memory>

#include "Engine/Core/ErrorCodes.hpp"
#include "Engine/Core/Logger.hpp"
#include "Engine/EngineState.hpp"
#include "Engine/Graphics/FrameInfo.hpp"
#include "Engine/Graphics/GpuProfiler.hpp"

#include "Editor/ui/CameraPanel.hpp"
#include "Editor/ui/DebugPanel.hpp"
#include "Editor/ui/IBLPanel.hpp"
#include "Editor/ui/PostProcessPanel.hpp"
#include "entt/entity/fwd.hpp"

namespace engine {

    SettingsPanel::SettingsPanel(EngineState* engineState, bool& multithreadedRecordingEnabled, uint32_t& multithreadedRecordingThreads, int& debugMode)
        : engineState_(engineState), multithreadedRecordingEnabled_(multithreadedRecordingEnabled), multithreadedRecordingThreads_(multithreadedRecordingThreads) {
        // CameraPanel still expects entt::entity + Scene*
        entt::entity camEntity = entt::null;
        Scene* scene = nullptr;
        if (engineState_ != nullptr) {
            auto sceneState = engineState_->sceneRuntimeService().view();
            if (sceneState.cameraEntity != nullptr) {
                camEntity = *sceneState.cameraEntity;
            }
            scene = sceneState.scene;
        }

        cameraPanel_           = std::make_unique<CameraPanel>(camEntity, scene);
        iblPanel_              = std::make_unique<IBLPanel>(engineState_);
        postProcessPanel_      = std::make_unique<PostProcessPanel>(engineState_->postProcessPushRef());
        debugPanel_            = std::make_unique<DebugPanel>(debugMode);
    }

    void SettingsPanel::render(FrameInfo& frameInfo) {
        if (!visible_) {
            return;
        }

        if (engineState_ == nullptr) {
            return;
        }

        auto rendering = engineState_->renderingService().view();
        auto sceneState = engineState_->sceneRuntimeService().view();
        auto resources = engineState_->resourceService().view();

        if (ImGui::Begin("Settings", &visible_)) {
            ImGui::Checkbox("Show Skybox", rendering.showSkybox);
            ImGui::SameLine();
            ImGui::Checkbox("Show Grid", rendering.showGrid);
            ImGui::SameLine();
            if (ImGui::Button((*rendering.showDebugObjects) ? "Hide Debug Objects" : "Show Debug Objects")) {
                *rendering.showDebugObjects = !(*rendering.showDebugObjects);
            }
            if ((*rendering.showSkybox) && (sceneState.skybox == nullptr)) {
                ImGui::TextDisabled("(Skybox will load next frame)");
            }
            ImGui::Separator();

            if (ImGui::CollapsingHeader("Sky")) {
                ImGui::Checkbox("Debug Cubemap Faces", &sceneState.skySettings->debugCubemapFaces);
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
                ImGui::Text("Cache Metrics");

                if (rendering.modelRenderSystem != nullptr) {
                    auto const     stats   = rendering.modelRenderSystem->getMaterialDescriptorCacheStats();
                    uint64_t const total   = stats.cacheHits + stats.cacheMisses;
                    double const   hitRate = (total > 0) ? (100.0 * static_cast<double>(stats.cacheHits) / static_cast<double>(total)) : 0.0;

                    ImGui::Text("Material Descriptor Cache");
                    ImGui::Text("  Hits: %llu  Misses: %llu  Hit Rate: %.1f%%", static_cast<unsigned long long>(stats.cacheHits), static_cast<unsigned long long>(stats.cacheMisses), hitRate);
                    ImGui::Text("  Buffer Writes: %llu", static_cast<unsigned long long>(stats.bufferWrites));
                }

                if (resources.resourceManager != nullptr) {
                    auto const     samplerStats = resources.resourceManager->getDevice().getSamplerCacheStats();
                    uint64_t const total        = samplerStats.cacheHits + samplerStats.cacheMisses;
                    double const   hitRate      = (total > 0) ? (100.0 * static_cast<double>(samplerStats.cacheHits) / static_cast<double>(total)) : 0.0;

                    ImGui::Text("Sampler Cache");
                    ImGui::Text("  Hits: %llu  Misses: %llu  Hit Rate: %.1f%%", static_cast<unsigned long long>(samplerStats.cacheHits), static_cast<unsigned long long>(samplerStats.cacheMisses), hitRate);
                    ImGui::Text("  Cached Samplers: %llu", static_cast<unsigned long long>(samplerStats.cachedSamplers));
                }

                if ((rendering.modelRenderSystem != nullptr) && ImGui::Button("Reset Material Cache Stats")) {
                    rendering.modelRenderSystem->resetMaterialDescriptorCacheStats();
                }
            }

            if (ImGui::CollapsingHeader("Shader Variants (Week 10)")) {
                if (rendering.modelRenderSystem == nullptr) {
                    ImGui::TextDisabled("ModelRenderSystem is not available.");
                } else {
                    int                          variantPolicy = static_cast<int>(rendering.modelRenderSystem->variantPolicy());
                    static constexpr const char* variantItems  = "Auto\0Force Standard\0Force Full\0";
                    if (ImGui::Combo("Variant Policy", &variantPolicy, variantItems)) {
                        variantPolicy = std::clamp(variantPolicy, 0, 2);
                        rendering.modelRenderSystem->setVariantPolicy(static_cast<ModelRenderSystem::VariantPolicy>(variantPolicy));
                    }
                    ImGui::SetItemTooltip("Auto chooses per-material; forced modes pin all transparent/transmission rendering to one variant.");

                    bool hotReloadEnabled = rendering.modelRenderSystem->shaderHotReloadEnabled();
                    if (ImGui::Checkbox("Shader Hot Reload", &hotReloadEnabled)) {
                        rendering.modelRenderSystem->setShaderHotReloadEnabled(hotReloadEnabled);
                    }

                    if (rendering.modelRenderSystem->standardVariantFallbackActive()) {
                        ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.2f, 1.0f), "Standard variant fallback active");
                        ImGui::TextWrapped("%s", rendering.modelRenderSystem->standardVariantFallbackReason().c_str());
                    } else {
                        ImGui::TextDisabled("Standard variant is available.");
                    }
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
