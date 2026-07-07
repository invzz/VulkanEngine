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
        uint32_t& multithreadedRecordingThreads, int& debugMode,
        bool& rtDirectional, bool& rtPoint, bool& rtSpot,
        float& rtShadowSoftness)
        : engineState_(engineState),
          multithreadedRecordingEnabled_(multithreadedRecordingEnabled),
          multithreadedRecordingThreads_(multithreadedRecordingThreads),
          rtDirectional_(rtDirectional),
          rtPoint_(rtPoint),
          rtSpot_(rtSpot),
          rtShadowSoftness_(rtShadowSoftness) {
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
        ImGuiViewport* viewport    = ImGui::GetMainViewport();
        ImVec2         defaultSize = ImVec2(560.0f, 620.0f);
        ImVec2         centeredPos = ImVec2(
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
            ImDrawList*  fg     = ImGui::GetForegroundDrawList(viewport);
            ImVec2 const vMin   = viewport->Pos;
            ImVec2 const vMax   = ImVec2(viewport->Pos.x + viewport->Size.x, viewport->Pos.y + viewport->Size.y);
            ImVec2 const wMin   = ImGui::GetWindowPos();
            ImVec2 const wMax   = ImVec2(wMin.x + ImGui::GetWindowSize().x, wMin.y + ImGui::GetWindowSize().y);
            ImU32 const  dimCol = IM_COL32(4, 8, 18, 110);
            fg->AddRectFilled(vMin, ImVec2(vMax.x, wMin.y), dimCol);
            fg->AddRectFilled(ImVec2(vMin.x, wMax.y), vMax, dimCol);
            fg->AddRectFilled(ImVec2(vMin.x, wMin.y), ImVec2(wMin.x, wMax.y), dimCol);
            fg->AddRectFilled(ImVec2(wMax.x, wMin.y), ImVec2(vMax.x, wMax.y), dimCol);
            ui::UI::BeginSurface("settings_scene", "Scene", "Global visibility and diagnostics");
            ui::UI::CheckboxRow("Show Skybox", "Display environment background", &engineState_->showSkybox());
            ui::UI::CheckboxRow("Show Grid", "Display editor reference grid", &engineState_->showGrid());
            ui::UI::CheckboxRow("Show Debug Objects", "Render helpers and debug geometry", &engineState_->showDebugObjects());
            ui::UI::EndSurface();
            ui::UI::BeginSurface("settings_sky", "Sky", "Cubemap and procedural sky controls");
            ui::UI::CheckboxRow("Procedural Sky", "Use procedural sky instead of cubemap",
                &engineState_->skySettings().proceduralSky);
            if (engineState_->skySettings().proceduralSky) {
                ui::UI::CheckboxRow("Use Sky LUT", "Use precomputed atmosphere LUT for procedural sky",
                    &engineState_->skySettings().useSkyLUT);
                ImGui::SetNextItemWidth(-1);
                ImGui::SliderFloat("Time of Day##sky_time", &engineState_->skySettings().timeOfDay, 0.0f, 24.0f, "%.1f h");
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("0 = midnight, 6 = sunrise, 12 = noon, 18 = sunset");
                ImGui::Separator();
                ImGui::Text("Sky Mode");
                ImGui::Separator();
                static const char* skyModeLabels[] = {"None", "Procedural", "Cubemap"};
                static int skyModeCur = (int)engineState_->skySettings().skyMode;
                ImGui::PushItemWidth(-1);
                if (ImGui::Combo("##skyMode", &skyModeCur, skyModeLabels, 3)) {
                    engineState_->skySettings().skyMode = (SkyMode)skyModeCur;
                }
            }
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
            ui::UI::BeginSurface("settings_raytracing", "Ray Tracing", "Per-light-type RT shadow toggles and softness");
            ui::UI::CheckboxRow("Directional", "Ray-traced shadows for directional lights", &rtDirectional_);
            ui::UI::CheckboxRow("Point", "Ray-traced shadows for point lights", &rtPoint_);
            ui::UI::CheckboxRow("Spot", "Ray-traced shadows for spot lights", &rtSpot_);
            ImGui::Separator();
            ImGui::Text("Shadow Softness");
            ImGui::SetNextItemWidth(-1);
            ImGui::SliderFloat("##shadowSoftness", &rtShadowSoftness_, 0.0f, 0.1f, "%.4f rad");
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Angular spread for soft shadow penumbra (0=hard, ~0.01=soft sun, 0.05=very soft)");
            ui::UI::EndSurface();
        }
        ImGui::End();
        wasVisibleLastFrame_ = visible_;
        ui::UI::PopThemeStyle();
    }
}  // namespace engine
