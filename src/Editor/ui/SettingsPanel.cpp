#include "Editor/ui/SettingsPanel.hpp"

#include <imgui.h>

#include "Engine/EngineState.hpp"

#include "Editor/ui/CameraPanel.hpp"
#include "Editor/ui/DebugPanel.hpp"
#include "Editor/ui/IBLPanel.hpp"
#include "Editor/ui/PostProcessPanel.hpp"
#include "Editor/ui/UI.hpp"

namespace engine {

    SettingsPanel::SettingsPanel(EngineState* engineState, bool& mtEnabled,
        uint32_t& mtThreads, int& debugMode)
        : engineState_(engineState),
          multithreadedRecordingEnabled_(mtEnabled),
          multithreadedRecordingThreads_(mtThreads) {
        cameraPanel_      = std::make_unique<CameraPanel>(*engineState_);
        iblPanel_         = std::make_unique<IBLPanel>(&engineState_->system<IBLSystem>());
        postProcessPanel_ = std::make_unique<PostProcessPanel>(engineState_->postProcess());
        debugPanel_       = std::make_unique<DebugPanel>(debugMode);
    }

    void SettingsPanel::render(FrameInfo& frameInfo) {
        if (!visible_ || engineState_ == nullptr)
            return;

        // Window title "Settings" matches the registry key in app.cpp so
        // DockBuilderDockWindow("Settings", nodeId) actually anchors this window.
        ui::UI::PushThemeStyle();
        if (ImGui::Begin("Settings", &visible_)) {
            ui::UI::Checkbox("Show Skybox##settings_skybox", &engineState_->showSkybox());
            ui::UI::Checkbox("Show Grid##settings_grid", &engineState_->showGrid());

            std::string dbgLabel = engineState_->showDebugObjects() ? "Hide Debug Objects" : "Show Debug Objects";
            if (ui::UI::Button(dbgLabel.c_str()))
                engineState_->showDebugObjects() = !engineState_->showDebugObjects();

            ui::UI::Separator();

            if (ui::UI::Section("Sky")) {
                ui::UI::Checkbox("Debug Cubemap Faces##sky_cubemap",
                    &engineState_->skySettings().debugCubemapFaces);
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

            if (ui::UI::Section("Profiling")) {
                debugPanel_->render(frameInfo);
            }

            // Multithreaded recording toggle
            if (ui::UI::Section("Rendering")) {
                ui::UI::Checkbox("Multithreaded Recording", &multithreadedRecordingEnabled_);
                if (multithreadedRecordingEnabled_) {
                    int threadCount = static_cast<int>(multithreadedRecordingThreads_);
                    if (ui::UI::InputInt("Thread Count", &threadCount, 1, 1)) {
                        if (threadCount > 0)
                            multithreadedRecordingThreads_ = static_cast<uint32_t>(threadCount);
                    }
                }
            }
        }
        ImGui::End();
        ui::UI::PopThemeStyle();
    }

}  // namespace engine
