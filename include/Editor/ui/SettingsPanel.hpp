#ifndef EDITOR_SETTINGSPANEL_HPP
#define EDITOR_SETTINGSPANEL_HPP

#include <memory>

#include "Engine/Graphics/FrameInfo.hpp"

#include "Editor/ui/CameraPanel.hpp"
#include "Editor/ui/DebugPanel.hpp"
#include "Editor/ui/IBLPanel.hpp"
#include "Editor/ui/PostProcessPanel.hpp"
#include "Editor/ui/UIPanel.hpp"

namespace engine {

    class EngineState;

    class SettingsPanel : public UIPanel {
       public:
        SettingsPanel(EngineState* engineState, bool& multithreadedRecordingEnabled, uint32_t& multithreadedRecordingThreads, int& debugMode);
        void render(FrameInfo& frameInfo) override;

        EngineState* engineState_ = nullptr;

        std::unique_ptr<CameraPanel>      cameraPanel_;
        std::unique_ptr<IBLPanel>         iblPanel_;
        std::unique_ptr<PostProcessPanel> postProcessPanel_;
        std::unique_ptr<DebugPanel>       debugPanel_;

        // Demo UI control for multithreaded recording (references owned by App)
        bool&     multithreadedRecordingEnabled_;
        uint32_t& multithreadedRecordingThreads_;
    };

}  // namespace engine

#endif  // EDITOR_SETTINGSPANEL_HPP
