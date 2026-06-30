#ifndef EDITOR_TOOLBARPANEL_HPP
#define EDITOR_TOOLBARPANEL_HPP

#include <string>
#include <vector>

#include "Engine/Graphics/FrameInfo.hpp"

#include "Editor/ui/UIPanel.hpp"

namespace engine {

    class EngineState;

    /**
 * @brief Top toolbar panel with panel visibility toggles, FPS display, and quick actions.
 *
 * Renders as a thin horizontal bar at the top of the viewport. Each dockable panel
 * gets a toggle button so the user can show/hide it without dragging headers away.
 */
    class ToolbarPanel : public UIPanel {
       public:
        ToolbarPanel();
        ~ToolbarPanel() override = default;

        void render(FrameInfo& frameInfo) override;

        /** Register a panel that should appear as a toggle button in the toolbar. */
        void addToggle(const std::string& label, UIPanel* panel);

        /** Switch to a predefined ImGui style preset. */
        void setStylePreset(int preset);
        int  getStylePreset() const {
            return stylePreset_;
        }

        /** Get the current frame time in milliseconds. */
        void setFrameTime(float ms) {
            frameTimeMs_ = ms;
        }

       private:
        struct ToggleEntry {
            std::string label;
            UIPanel*    panel;
        };

        std::vector<ToggleEntry> toggles_;
        int                      stylePreset_ = 0;  // 0=dark, 1=light, 2=midnight
        float                    frameTimeMs_ = 0.0f;

        /** Apply an ImGui style preset. */
        void applyStylePreset(int preset);
    };

}  // namespace engine

#endif  // EDITOR_TOOLBARPANEL_HPP
