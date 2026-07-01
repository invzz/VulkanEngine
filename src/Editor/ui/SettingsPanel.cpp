#include "Editor/ui/SettingsPanel.hpp"

#include <imgui.h>

#include "Engine/EngineState.hpp"

#include "Editor/ui/CameraPanel.hpp"
#include "Editor/ui/DebugPanel.hpp"
#include "Editor/ui/IBLPanel.hpp"
#include "Editor/ui/PostProcessPanel.hpp"
#include "Editor/ui/UI.hpp"

namespace engine {

        SettingsPanel::SettingsPanel(EngineState* engineState, bool& multithreadedRecordingEnabled,
                uint32_t& multithreadedRecordingThreads, int& debugMode)
        : engineState_(engineState),
                    multithreadedRecordingEnabled_(multithreadedRecordingEnabled),
                    multithreadedRecordingThreads_(multithreadedRecordingThreads) {
        cameraPanel_      = std::make_unique<CameraPanel>(*engineState_);
        iblPanel_         = std::make_unique<IBLPanel>(&engineState_->system<IBLSystem>());
        postProcessPanel_ = std::make_unique<PostProcessPanel>(engineState_->postProcess());
        debugPanel_       = std::make_unique<DebugPanel>(debugMode);
    }

    void SettingsPanel::render(FrameInfo& frameInfo) {
        if (!visible_ || engineState_ == nullptr) {
            return;
        }

        // Window title "Settings" matches the registry key in app.cpp so
        // DockBuilderDockWindow("Settings", nodeId) actually anchors this window.
        ui::UI::PushThemeStyle();
        if (ImGui::Begin("Settings", &visible_)) {
            ui::UI::BeginSurface("settings_scene", "Scene", "Global visibility and diagnostics");
            ui::UI::CheckboxRow("Show Skybox", "Display environment background", &engineState_->showSkybox());
            ui::UI::CheckboxRow("Show Grid", "Display editor reference grid", &engineState_->showGrid());
            ui::UI::CheckboxRow("Show Debug Objects", "Render helpers and debug geometry", &engineState_->showDebugObjects());
            ui::UI::EndSurface();

            ui::UI::BeginSurface("settings_sky", "Sky", "Cubemap visualization controls");
            ui::UI::CheckboxRow("Debug Cubemap Faces", "Display cubemap face index tinting",
                &engineState_->skySettings().debugCubemapFaces);
            ui::UI::EndSurface();

            ui::UI::BeginSurface("settings_camera", "Camera", "Projection and movement tuning");
            cameraPanel_->render(frameInfo);
            ui::UI::EndSurface();

            ui::UI::BeginSurface("settings_ibl", "Environment (IBL)", "Image-based lighting probes");
            iblPanel_->render(frameInfo);
            ui::UI::EndSurface();

            ui::UI::BeginSurface("settings_post", "Post Processing", "Final image adjustments");
            postProcessPanel_->render(frameInfo);
            ui::UI::EndSurface();

            ui::UI::BeginSurface("settings_profiling", "Profiling", "Debug visualizations and frame diagnostics");
            debugPanel_->render(frameInfo);
            ui::UI::EndSurface();

            ui::UI::BeginSurface("settings_rendering", "Rendering", "Command recording behavior");
            ui::UI::CheckboxRow("Multithreaded Recording", "Build command buffers on worker threads",
                &multithreadedRecordingEnabled_);
            if (multithreadedRecordingEnabled_) {
                int threadCount = static_cast<int>(multithreadedRecordingThreads_);
                if (ui::UI::InputInt("Thread Count##settings_threads", &threadCount, 1, 1)) {
                    if (threadCount > 0) {
                        multithreadedRecordingThreads_ = static_cast<uint32_t>(threadCount);
                    }
                }
            }
            ui::UI::EndSurface();
        }
        ImGui::End();
        ui::UI::PopThemeStyle();
    }

}  // namespace engine
