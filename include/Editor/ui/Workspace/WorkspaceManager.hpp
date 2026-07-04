#ifndef EDITOR_WORKSPACE_WORKSPACE_MANAGER_HPP
#define EDITOR_WORKSPACE_WORKSPACE_MANAGER_HPP
#include <memory>
#include <string>
#include <vector>

#include "Engine/Graphics/FrameInfo.hpp"

#include "Editor/ui/UIPanel.hpp"
#include "Layout.hpp"
#include "LayoutBuilder.hpp"
#include "PanelRegistry.hpp"
#include "ThemeSystem.hpp"
#include "UIState.hpp"
namespace engine {
    class ImGuiManager;
    /**
 * @brief Central manager for editor workspace layout, panels, and state.
 *
 * This is the top-level orchestrator for the editor's UI architecture.
 * It manages:
 * - Panel docking and layout rules
 * - Theme/styling
 * - Workspace state (selection, active camera, etc.)
 * - Rendering of the dockspace and panels
 * - Layout validation and constraint enforcement
 *
 * Panels should NOT be rendered directly from App. Instead, App calls
 * UIManager::render() which delegates to WorkspaceManager.
 */
    class WorkspaceManager {
       public:
        WorkspaceManager();
        ~WorkspaceManager() = default;
        /**
     * @brief Initialize the workspace manager.
     * Called once during App::init().
     */
        void initialize(ImGuiManager& imguiManager);
        /**
     * @brief Get the panel registry.
     */
        PanelRegistry& getPanelRegistry() {
            return panelRegistry_;
        }
        /**
     * @brief Get the UI state.
     */
        UIState& getUIState() {
            return uiState_;
        }
        /**
     * @brief Get the theme system.
     */
        ThemeSystem& getThemeSystem() {
            return themeSystem_;
        }
        /**
     * @brief Render the workspace (dockspace + all visible panels).
     * Called every frame from UIManager::render().
     */
        void render(FrameInfo& frameInfo, VkCommandBuffer commandBuffer, bool drawUI);
        /**
     * @brief Apply a layout preset.
     *
     * Tears down the current dock tree and rebuilds it from the registered
     * panels' DockConstraints (via LayoutBuilder). Safe to call any time;
     * typically called once on first frame or when the user clicks "Reset
     * Layout" in the toolbar.
     */
        void applyLayoutPreset(LayoutPreset preset);
        /**
     * @brief Reset the layout to the current preset.
     *
     * Convenience alias for applyLayoutPreset(getCurrentLayout()).
     */
        void resetLayout();
        /**
     * @brief Register a panel with its docking constraints.
     * Convenience wrapper around panelRegistry_.registerPanel().
     */
        void registerPanel(const std::string& name, std::unique_ptr<UIPanel> panel,
            DockConstraints constraints = DockConstraints{});
        /**
     * @brief Get the current toolbar visibility.
     */
        bool isToolbarVisible() const {
            return toolbarVisible_;
        }
        /**
     * @brief Set the toolbar visibility.
     */
        void setToolbarVisible(bool visible) {
            toolbarVisible_ = visible;
        }
        /**
     * @brief Get the toolbar panel (if registered).
     */
        class ToolbarPanel* getToolbarPanel() const {
            return toolbarPanel_.get();
        }
        /**
     * @brief Set the toolbar panel.
     */
        void setToolbarPanel(std::unique_ptr<class ToolbarPanel> toolbar);
        /**
     * @brief Add a toggle for a panel in the toolbar.
     */
        void addToolbarToggle(const std::string& label, UIPanel* panel);
        /**
     * @brief Set the frame time for the toolbar FPS display.
     */
        void setFrameTimeMs(float ms);
        /**
     * @brief Get the main dockspace ID.
     */
        ImGuiID getDockspaceID() const {
            return mainDockspaceID_;
        }
        /**
     * @brief Get the viewport panel (future).
     */
        static class ViewportPanel* getViewportPanel() {
            return nullptr;
        }
        /**
     * @brief Get the scene hierarchy panel (future).
     */
        class SceneHierarchyPanel* getSceneHierarchyPanel() const {
            return nullptr;
        }
        /**
     * @brief Get the inspector panel (future).
     */
        class InspectorPanel* getInspectorPanel() const {
            return nullptr;
        }
        /**
     * @brief Get the console panel (future).
     */
        class ConsolePanel* getConsolePanel() const {
            return nullptr;
        }
        /**
     * @brief Get the asset browser panel (future).
     */
        class AssetBrowserPanel* getAssetBrowserPanel() const {
            return nullptr;
        }
        /**
     * @brief Validate the current layout and enforce workspace constraints.
     *
     * This method checks all registered panels against their docking constraints
     * and ensures they follow the workspace layout rules. It is called every
     * frame during render() to maintain layout integrity.
     *
     * @return true if all constraints are satisfied, false if violations were found
     *         and corrected.
     */
        bool validateLayout();
        /**
     * @brief Enforce docking constraints for a specific panel.
     *
     * This method ensures a panel's docking constraints are valid and
     * applies corrections if needed. It is called by validateLayout() for
     * each registered panel.
     *
     * @param name The panel name.
     * @param constraints The docking constraints to enforce.
     * @return true if constraints are valid, false if they were corrected.
     */
        bool enforceConstraints(const std::string& name, const DockConstraints& constraints);
        /**
     * @brief Get the current layout preset.
     */
        LayoutPreset getCurrentLayout() const {
            return currentLayout_;
        }
        /**
     * @brief Set the current layout preset.
     */
        void setCurrentLayout(LayoutPreset preset) {
            currentLayout_ = preset;
        }

       private:
        ImGuiManager*                       imguiManager_ = nullptr;
        PanelRegistry                       panelRegistry_;
        UIState                             uiState_;
        ThemeSystem                         themeSystem_;
        bool                                toolbarVisible_ = true;
        std::unique_ptr<class ToolbarPanel> toolbarPanel_;
        struct ToolbarToggleEntry {
            std::string label;
            UIPanel*    panel;
        };
        std::vector<ToolbarToggleEntry> toolbarToggles_;
        float                           frameTimeMs_     = 0.0f;
        ImGuiID                         mainDockspaceID_ = 0;
        LayoutPreset                    currentLayout_   = LayoutPreset::Default;
        bool                            layoutApplied_   = false;
        class ViewportPanel;
        class SceneHierarchyPanel;
        class InspectorPanel;
        class ConsolePanel;
        class AssetBrowserPanel;
    };
}  // namespace engine
#endif
