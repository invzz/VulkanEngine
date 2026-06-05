#ifndef EDITOR_WORKSPACE_WORKSPACE_MANAGER_HPP
#define EDITOR_WORKSPACE_WORKSPACE_MANAGER_HPP

#include <memory>
#include <string>
#include <vector>
#include <functional>

#include "Editor/Workspace/Layout.hpp"
#include "Editor/Workspace/PanelRegistry.hpp"
#include "Editor/Workspace/ThemeSystem.hpp"
#include "Editor/Workspace/UIState.hpp"
#include "Editor/ui/UIPanel.hpp"
#include "Engine/Graphics/FrameInfo.hpp"

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
    PanelRegistry& getPanelRegistry() { return panelRegistry_; }

    /**
     * @brief Get the UI state.
     */
    UIState& getUIState() { return uiState_; }

    /**
     * @brief Get the theme system.
     */
    ThemeSystem& getThemeSystem() { return themeSystem_; }

    /**
     * @brief Render the workspace (dockspace + all visible panels).
     * Called every frame from UIManager::render().
     */
    void render(FrameInfo& frameInfo, VkCommandBuffer commandBuffer, bool drawUI);

    /**
     * @brief Apply a layout preset.
     */
    void applyLayoutPreset(LayoutPreset preset);

    /**
     * @brief Register a panel with its docking constraints.
     * Convenience wrapper around panelRegistry_.registerPanel().
     */
    void registerPanel(const std::string& name, std::unique_ptr<UIPanel> panel,
                       DockConstraints constraints = DockConstraints{});

    /**
     * @brief Get the current toolbar visibility.
     */
    bool isToolbarVisible() const { return toolbarVisible_; }

    /**
     * @brief Set the toolbar visibility.
     */
    void setToolbarVisible(bool visible) { toolbarVisible_ = visible; }

    /**
     * @brief Get the toolbar panel (if registered).
     */
    class ToolbarPanel* getToolbarPanel() const { return toolbarPanel_.get(); }

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
    ImGuiID getDockspaceID() const { return mainDockspaceID_; }

    /**
     * @brief Get the viewport panel (future).
     */
    class ViewportPanel* getViewportPanel() const { return nullptr; }

    /**
     * @brief Get the scene hierarchy panel (future).
     */
    class SceneHierarchyPanel* getSceneHierarchyPanel() const { return nullptr; }

    /**
     * @brief Get the inspector panel (future).
     */
    class InspectorPanel* getInspectorPanel() const { return nullptr; }

    /**
     * @brief Get the console panel (future).
     */
    class ConsolePanel* getConsolePanel() const { return nullptr; }

    /**
     * @brief Get the asset browser panel (future).
     */
    class AssetBrowserPanel* getAssetBrowserPanel() const { return nullptr; }

private:
    ImGuiManager* imguiManager_ = nullptr;
    PanelRegistry panelRegistry_;
    UIState       uiState_;
    ThemeSystem   themeSystem_;

    // Toolbar
    bool          toolbarVisible_ = true;
    std::unique_ptr<class ToolbarPanel> toolbarPanel_;
    struct ToolbarToggleEntry {
        std::string label;
        UIPanel*    panel;
    };
    std::vector<ToolbarToggleEntry> toolbarToggles_;

    // Frame time for toolbar display
    float frameTimeMs_ = 0.0f;

    // Dockspace
    ImGuiID mainDockspaceID_ = 0;

    // Layout rules
    LayoutPreset currentLayout_ = LayoutPreset::Default;

    // Forward declarations for panel getters
    class ViewportPanel;
    class SceneHierarchyPanel;
    class InspectorPanel;
    class ConsolePanel;
    class AssetBrowserPanel;
};

}  // namespace engine

#endif  // EDITOR_WORKSPACE_WORKSPACE_MANAGER_HPP
