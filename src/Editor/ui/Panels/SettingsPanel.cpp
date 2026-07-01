#include "Editor/ui/Panels/SettingsPanel.hpp"

#include <imgui.h>

#include "Engine/EngineState.hpp"

#include "Editor/ui/Panels/CameraPanel.hpp"
#include "Editor/ui/Panels/DebugPanel.hpp"
#include "Editor/ui/Panels/IBLPanel.hpp"
#include "Editor/ui/Panels/PostProcessPanel.hpp"
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
            wasVisibleLastFrame_ = false;
            return;
        }

        bool openingNow = !wasVisibleLastFrame_;

        ui::UI::PushThemeStyle();

        ImGuiViewport* viewport = ImGui::GetMainViewport();

        ImVec2 defaultSize = ImVec2(560.0f, 620.0f);
        ImVec2 centeredPos = ImVec2(
            viewport->WorkPos.x + ((viewport->WorkSize.x - defaultSize.x) * 0.5f),
            viewport->WorkPos.y + ((viewport->WorkSize.y - defaultSize.y) * 0.5f));

        ImGui::SetNextWindowViewport(viewport->ID);
        ImGui::SetNextWindowSizeConstraints(ImVec2(420.0f, 320.0f), ImVec2(viewport->WorkSize.x, viewport->WorkSize.y));
        if (openingNow) {
            ImGui::SetNextWindowPos(centeredPos, ImGuiCond_Always);
            ImGui::SetNextWindowSize(defaultSize, ImGuiCond_Always);
            ImGui::SetNextWindowFocus();
        }

        ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoDocking;

        if (ImGui::Begin("Settings", &visible_, windowFlags)) {
            // Subtle dimmer for modal-like utility focus, excluding settings window itself.
            ImDrawList*  fg     = ImGui::GetForegroundDrawList(viewport);
            ImVec2 const vMin   = viewport->Pos;
            ImVec2 const vMax   = ImVec2(viewport->Pos.x + viewport->Size.x, viewport->Pos.y + viewport->Size.y);
            ImVec2 const wMin   = ImGui::GetWindowPos();
            ImVec2 const wMax   = ImVec2(wMin.x + ImGui::GetWindowSize().x, wMin.y + ImGui::GetWindowSize().y);
            ImU32 const  dimCol = IM_COL32(4, 8, 18, 110);

            // Top
            fg->AddRectFilled(vMin, ImVec2(vMax.x, wMin.y), dimCol);
            // Bottom
            fg->AddRectFilled(ImVec2(vMin.x, wMax.y), vMax, dimCol);
            // Left
            fg->AddRectFilled(ImVec2(vMin.x, wMin.y), ImVec2(wMin.x, wMax.y), dimCol);
            // Right
            fg->AddRectFilled(ImVec2(wMax.x, wMin.y), ImVec2(vMax.x, wMax.y), dimCol);

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
        wasVisibleLastFrame_ = visible_;
        ui::UI::PopThemeStyle();
    }

}  // namespace engine
