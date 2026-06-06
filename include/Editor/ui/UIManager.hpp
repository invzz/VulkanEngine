#ifndef EDITOR_UIMANAGER_HPP
#define EDITOR_UIMANAGER_HPP

#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "Engine/Graphics/FrameInfo.hpp"
#include "Engine/Graphics/ImGuiManager.hpp"

#include "Editor/Workspace/WorkspaceManager.hpp"
#include "Editor/ui/UIPanel.hpp"

namespace engine {

    class ToolbarPanel;

    /**
 * @brief Manages all UI panels.
 *
 * Delegates to WorkspaceManager for layout, theming, and state management.
 * Maintains backward-compatible API for existing panel registration.
 */
    class UIManager {
       public:
        explicit UIManager(ImGuiManager& imguiManager);

        /**
   * @brief Add a panel to the manager
   */
        void addPanel(std::unique_ptr<UIPanel> panel);

        /**
   * @brief Render all panels
   */
        void render(FrameInfo& frameInfo, VkCommandBuffer commandBuffer);
        void render(FrameInfo& frameInfo, VkCommandBuffer commandBuffer, bool drawUI);

        /**
   * @brief Get a specific panel by type (returns nullptr if not found)
   */
        template <typename T>
        T* getPanel() {
            return workspaceManager_.getPanelRegistry().getPanel<T>();
        }

        void setOnSaveScene(std::function<void()> callback) {
            onSaveScene_ = std::move(callback);
        }
        void setOnLoadScene(std::function<void()> callback) {
            onLoadScene_ = std::move(callback);
        }

        /** Set the toolbar panel (rendered as a thin top bar). */
        void setToolbarPanel(std::unique_ptr<ToolbarPanel> toolbar);

        /** Register a panel toggle in the toolbar. */
        void addToolbarToggle(const std::string& label, UIPanel* panel);

        /** Get the underlying WorkspaceManager for advanced access. */
        WorkspaceManager& getWorkspaceManager() {
            return workspaceManager_;
        }

        /** Get the UI state. */
        UIState& getUIState() {
            return workspaceManager_.getUIState();
        }

        /** Get the theme system. */
        ThemeSystem& getThemeSystem() {
            return workspaceManager_.getThemeSystem();
        }

        /** Get the panel registry. */
        PanelRegistry& getPanelRegistry() {
            return workspaceManager_.getPanelRegistry();
        }

       private:
        WorkspaceManager      workspaceManager_;
        std::function<void()> onSaveScene_;
        std::function<void()> onLoadScene_;
    };
}  // namespace engine

#endif  // EDITOR_UIMANAGER_HPP
