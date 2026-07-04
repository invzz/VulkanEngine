#ifndef EDITOR_TOOLBARPANEL_HPP
#define EDITOR_TOOLBARPANEL_HPP
#include <functional>
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
        /** Get the preferred toolbar height for the current DPI and viewport width. */
        float getPreferredHeight(float viewportWidth) const;
        /** Register a panel that should appear as a toggle button in the toolbar. */
        void addToggle(const std::string& label, UIPanel* panel);
        /** Register the floating settings panel launched from toolbar gear button. */
        void setSettingsPanel(UIPanel* panel) {
            settingsPanel_ = panel;
        }
        /** Switch to a predefined ImGui style preset. */
        void setStylePreset(int preset);
        int  getStylePreset() const {
            return stylePreset_;
        }
        /** Get the current frame time in milliseconds. */
        void setFrameTime(float ms) {
            frameTimeMs_ = ms;
        }
        /**
         * @brief Set the "Reset Layout" callback.
         *
         * The toolbar will render a "Reset" button that invokes this callback.
         * WorkspaceManager wires its own resetLayout() here so the user can
         * rebuild the dock tree at any time.
         */
        void setOnResetLayout(std::function<void()> callback) {
            onResetLayout_ = std::move(callback);
        }

       private:
        struct ToggleEntry {
            std::string label;
            UIPanel*    panel;
        };
        std::vector<ToggleEntry> toggles_;
        UIPanel*                 settingsPanel_ = nullptr;
        int                      stylePreset_   = 0;
        float                    frameTimeMs_   = 0.0f;
        std::function<void()>    onResetLayout_;
        /** Apply an ImGui style preset. */
        void applyStylePreset(int preset);
    };
}  // namespace engine
#endif
