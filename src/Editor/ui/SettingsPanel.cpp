#include "Editor/ui/SettingsPanel.hpp"

#include <algorithm>
#include <imgui.h>
#include <memory>

#include "Engine/Core/ErrorCodes.hpp"
#include "Engine/Core/Logger.hpp"
#include "Engine/EngineState.hpp"
#include "Engine/Graphics/FrameInfo.hpp"
#include "Engine/Graphics/GpuProfiler.hpp"

#include "Editor/UI/UI.hpp"
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
        Scene*       scene     = nullptr;
        if (engineState_ != nullptr) {
            auto sceneState = engineState_->sceneRuntimeService().view();
            if (sceneState.cameraEntity != nullptr) {
                camEntity = *sceneState.cameraEntity;
            }
            scene = sceneState.scene;
        }

        cameraPanel_      = std::make_unique<CameraPanel>(camEntity, scene);
        iblPanel_         = std::make_unique<IBLPanel>(engineState_);
        postProcessPanel_ = std::make_unique<PostProcessPanel>(engineState_->postProcessPushRef());
        debugPanel_       = std::make_unique<DebugPanel>(debugMode);
    }

    void SettingsPanel::render(FrameInfo& frameInfo) {
        if (!visible_) {
            return;
        }

        if (engineState_ == nullptr) {
            return;
        }

        // Push theme style
        ui::UI::PushThemeStyle();

        auto rendering  = engineState_->renderingService().view();
        auto sceneState = engineState_->sceneRuntimeService().view();
        auto resources  = engineState_->resourceService().view();

        if (ImGui::Begin("Settings", &visible_)) {
            // Top-level checkboxes
            ui::UI::Checkbox("Show Skybox##settings_skybox", rendering.showSkybox);
            ImGui::SameLine();
            ui::UI::Checkbox("Show Grid##settings_grid", rendering.showGrid);
            ImGui::SameLine();
            std::string btnLabel = (*rendering.showDebugObjects) ? "Hide Debug Objects" : "Show Debug Objects";
            if (ui::UI::Button(btnLabel.c_str())) {
                *rendering.showDebugObjects = !(*rendering.showDebugObjects);
            }
            if ((*rendering.showSkybox) && (sceneState.skybox == nullptr)) {
                ui::UI::TextDisabled("(Skybox will load next frame)");
            }
            ui::UI::Separator();

            // Collapsing sections
            if (ui::UI::Section("Sky")) {
                ui::UI::Checkbox("Debug Cubemap Faces##sky_cubemap", &sceneState.skySettings->debugCubemapFaces);
            }

            if (ui::UI::Section("Camera")) {
                cameraPanel_->render(frameInfo);
            }

            if (ui::UI::Section("Environment (IBL)")) {
                iblPanel_->render(frameInfo);
            }

            if (ui::UI::Section("Post Processing")) {
                postProcessPanel_->render(frameInfo);
            }

            if (ui::UI::Section("Debug")) {
                debugPanel_->render(frameInfo);
            }

            if (ui::UI::Section("Performance")) {
                // Multithreaded recording control (opt-in pilot)
                ui::UI::Checkbox("Multithreaded recording (secondary CB)##perf_mt", &multithreadedRecordingEnabled_);
                ui::UI::InfoTooltip("When enabled, draw-recording is partitioned across worker threads into secondary command buffers.");

                int tmpThreads = static_cast<int>(multithreadedRecordingThreads_);
                if (ImGui::InputInt("Recording threads (0 = auto)##perf_threads", &tmpThreads)) {
                    tmpThreads                     = std::max(tmpThreads, 0);
                    multithreadedRecordingThreads_ = static_cast<uint32_t>(tmpThreads);
                }
                ui::UI::InfoTooltip("0 = auto (HW threads - 1); set to 1 to force single-threaded serial recording.");

                ui::UI::Separator();
                ui::UI::TextDisabled("Cache Metrics");

                if (rendering.modelRenderSystem != nullptr) {
                    auto const     stats   = rendering.modelRenderSystem->getMaterialDescriptorCacheStats();
                    uint64_t const total   = stats.cacheHits + stats.cacheMisses;
                    double const   hitRate = (total > 0) ? (100.0 * static_cast<double>(stats.cacheHits) / static_cast<double>(total)) : 0.0;

                    std::string matText = "Material Descriptor Cache";
                    ui::UI::TextColored(matText.c_str(), ImVec4(0.9f, 0.9f, 0.9f, 1.0f));
                    std::string hitText = "  Hits: " + std::to_string(stats.cacheHits) + "  Misses: " + std::to_string(stats.cacheMisses) + "  Hit Rate: " + std::to_string(hitRate).substr(0, 5) + "%";
                    ui::UI::TextDisabled(hitText.c_str());
                    std::string bufText = "  Buffer Writes: " + std::to_string(stats.bufferWrites);
                    ui::UI::TextDisabled(bufText.c_str());
                }

                if (resources.resourceManager != nullptr) {
                    auto const     samplerStats = resources.resourceManager->getDevice().getSamplerCacheStats();
                    uint64_t const total        = samplerStats.cacheHits + samplerStats.cacheMisses;
                    double const   hitRate      = (total > 0) ? (100.0 * static_cast<double>(samplerStats.cacheHits) / static_cast<double>(total)) : 0.0;

                    std::string samplerText = "Sampler Cache";
                    ui::UI::TextColored(samplerText.c_str(), ImVec4(0.9f, 0.9f, 0.9f, 1.0f));
                    std::string hitText2 = "  Hits: " + std::to_string(samplerStats.cacheHits) + "  Misses: " + std::to_string(samplerStats.cacheMisses) + "  Hit Rate: " + std::to_string(hitRate).substr(0, 5) + "%";
                    ui::UI::TextDisabled(hitText2.c_str());
                    std::string cachedText = "  Cached Samplers: " + std::to_string(samplerStats.cachedSamplers);
                    ui::UI::TextDisabled(cachedText.c_str());
                }

                if ((rendering.modelRenderSystem != nullptr) && ui::UI::Button("Reset Material Cache Stats##perf_reset")) {
                    rendering.modelRenderSystem->resetMaterialDescriptorCacheStats();
                }
            }

            if (ui::UI::Section("Shader Variants (Week 10)")) {
                if (rendering.modelRenderSystem == nullptr) {
                    ui::UI::TextDisabled("ModelRenderSystem is not available.");
                } else {
                    int                          variantPolicy = static_cast<int>(rendering.modelRenderSystem->variantPolicy());
                    static constexpr const char* variantItems  = "Auto\0Force Standard\0Force Full\0";
                    if (ImGui::Combo("Variant Policy##shader_variant", &variantPolicy, variantItems)) {
                        variantPolicy = std::clamp(variantPolicy, 0, 2);
                        rendering.modelRenderSystem->setVariantPolicy(static_cast<ModelRenderSystem::VariantPolicy>(variantPolicy));
                    }
                    ui::UI::InfoTooltip("Auto chooses per-material; forced modes pin all transparent/transmission rendering to one variant.");

                    bool hotReloadEnabled = rendering.modelRenderSystem->shaderHotReloadEnabled();
                    if (ui::UI::Checkbox("Shader Hot Reload##shader_hotreload", &hotReloadEnabled)) {
                        rendering.modelRenderSystem->setShaderHotReloadEnabled(hotReloadEnabled);
                    }

                    if (rendering.modelRenderSystem->standardVariantFallbackActive()) {
                        ui::UI::TextColored("Standard variant fallback active", ImVec4(1.0f, 0.75f, 0.2f, 1.0f));
                        std::string reasonText = rendering.modelRenderSystem->standardVariantFallbackReason();
                        ui::UI::TextDisabled(reasonText.c_str());
                    } else {
                        ui::UI::TextDisabled("Standard variant is available.");
                    }
                }
            }

            if (ui::UI::Section("GPU Profiler")) {
                auto& profiler = GpuProfiler::instance();
                bool  enabled  = profiler.isEnabled();
                if (ui::UI::Checkbox("Enable Profiling##prof_enable", &enabled)) {
                    profiler.setEnabled(enabled);
                }

                std::string frameText = "Last Frame: " + std::to_string(profiler.lastFrameIndex());
                ui::UI::TextDisabled(frameText.c_str());
                std::string cpuText = "Frame CPU: " + std::to_string(profiler.lastFrameCpuMs()).substr(0, 6) + " ms";
                ui::UI::TextDisabled(cpuText.c_str());
                if (profiler.lastFrameGpuMs() >= 0.0) {
                    std::string gpuText = "Frame GPU (sum pass): " + std::to_string(profiler.lastFrameGpuMs()).substr(0, 6) + " ms";
                    ui::UI::TextDisabled(gpuText.c_str());
                } else {
                    ui::UI::TextDisabled("Frame GPU (sum pass): n/a");
                }
                ui::UI::Separator();

                for (const auto& timing : profiler.lastFramePassTimings()) {
                    if (timing.gpuMs >= 0.0) {
                        std::string passText = timing.passName + "  CPU: " + std::to_string(timing.cpuMs).substr(0, 6) + " ms  GPU: " + std::to_string(timing.gpuMs).substr(0, 6) + " ms";
                        ui::UI::TextDisabled(passText.c_str());
                    } else {
                        std::string passText = timing.passName + "  CPU: " + std::to_string(timing.cpuMs).substr(0, 6) + " ms  GPU: n/a";
                        ui::UI::TextDisabled(passText.c_str());
                    }
                }

                if (ui::UI::Button("Export Last Frame CSV##prof_csv")) {
                    std::string error;
                    if (!profiler.exportLastFrameCsv("gpu_profile_last_frame.csv", &error)) {
                        Logger::warn(LogChannel::Resource, "Failed to export GPU profiler snapshot: ", error);
                    } else {
                        Logger::info(LogChannel::Resource, "Exported GPU profiler snapshot to gpu_profile_last_frame.csv");
                    }
                }

                ImGui::SameLine();
                if (ui::UI::Button("Export Last Frame JSON##prof_json")) {
                    std::string error;
                    if (!profiler.exportLastFrameJson("gpu_profile_last_frame.json", &error)) {
                        Logger::warn(LogChannel::Resource, "Failed to export GPU profiler JSON snapshot: ", error);
                    } else {
                        Logger::info(LogChannel::Resource, "Exported GPU profiler snapshot to gpu_profile_last_frame.json");
                    }
                }
            }

            if (ui::UI::Section("Logging")) {
                int                          minLevel   = static_cast<int>(Logger::minimumLevel());
                static constexpr const char* levelItems = "Error\0Warn\0Info\0Debug\0";
                if (ImGui::Combo("Minimum Level##log_level", &minLevel, levelItems)) {
                    minLevel = std::clamp(minLevel, 0, 3);
                    Logger::setMinimumLevel(static_cast<LogLevel>(minLevel));
                }

                bool generalEnabled  = Logger::isChannelEnabled(LogChannel::General);
                bool renderEnabled   = Logger::isChannelEnabled(LogChannel::Render);
                bool syncEnabled     = Logger::isChannelEnabled(LogChannel::Sync);
                bool sceneEnabled    = Logger::isChannelEnabled(LogChannel::Scene);
                bool resourceEnabled = Logger::isChannelEnabled(LogChannel::Resource);

                if (ui::UI::Checkbox("General##log_general", &generalEnabled)) {
                    Logger::enableChannel(LogChannel::General, generalEnabled);
                }
                ImGui::SameLine();
                if (ui::UI::Checkbox("Render##log_render", &renderEnabled)) {
                    Logger::enableChannel(LogChannel::Render, renderEnabled);
                }
                ImGui::SameLine();
                if (ui::UI::Checkbox("Sync##log_sync", &syncEnabled)) {
                    Logger::enableChannel(LogChannel::Sync, syncEnabled);
                }

                if (ui::UI::Checkbox("Scene##log_scene", &sceneEnabled)) {
                    Logger::enableChannel(LogChannel::Scene, sceneEnabled);
                }
                ImGui::SameLine();
                if (ui::UI::Checkbox("Resource##log_resource", &resourceEnabled)) {
                    Logger::enableChannel(LogChannel::Resource, resourceEnabled);
                }
            }

            if (ui::UI::Section("Error Boundaries (Week 8)")) {
                uint64_t const recoverableCount = ErrorState::countByBoundary(ErrorBoundary::Recoverable);
                uint64_t const fatalCount       = ErrorState::countByBoundary(ErrorBoundary::Fatal);

                std::string recText = "Recoverable events: " + std::to_string(recoverableCount);
                ui::UI::TextDisabled(recText.c_str());
                std::string fatalText = "Fatal events: " + std::to_string(fatalCount);
                ui::UI::TextDisabled(fatalText.c_str());

                if (ui::UI::Button("Clear Error Events##errors_clear")) {
                    ErrorState::clear();
                }

                auto const recent = ErrorState::recentEvents(12);
                if (recent.empty()) {
                    ui::UI::TextDisabled("No error or fallback events recorded.");
                } else {
                    ui::UI::Separator();
                    for (const auto& event : recent) {
                        const char* boundaryLabel = (event.boundary == ErrorBoundary::Recoverable) ? "Recoverable" : "Fatal";
                        ImVec4      color         = (event.boundary == ErrorBoundary::Recoverable) ? ImVec4(1.0f, 0.8f, 0.2f, 1.0f) : ImVec4(1.0f, 0.3f, 0.3f, 1.0f);
                        std::string eventText     = "[`" + std::string(boundaryLabel) + "] code=" + std::to_string(static_cast<unsigned long long>(event.code)) + " count=" + std::to_string(static_cast<unsigned long long>(event.count));
                        ui::UI::TextColored(eventText.c_str(), color);
                        ui::UI::TextDisabled(event.message.c_str());
                    }
                }
            }
        }
        ImGui::End();

        ui::UI::PopThemeStyle();
    }

}  // namespace engine
