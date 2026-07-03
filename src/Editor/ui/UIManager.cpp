#include "Editor/ui/UIManager.hpp"

#include <memory>
#include <utility>

#include "Engine/Graphics/FrameInfo.hpp"
#include "Engine/Graphics/ImGuiManager.hpp"

#include "Editor/ui/Panels/ToolbarPanel.hpp"
#include "Editor/ui/UI.hpp"
#include "Editor/ui/UIPanel.hpp"
#include "Editor/ui/Workspace/WorkspaceManager.hpp"
#include "vulkan/vulkan_core.h"

namespace engine {

    UIManager::UIManager(ImGuiManager& imguiManager) {
        workspaceManager_.initialize(imguiManager);
    }

    void UIManager::addPanel(std::unique_ptr<UIPanel> panel) {
        UIPanel* const    panelPtr = panel.get();
        const std::string name     = (panelPtr != nullptr) ? typeid(*panelPtr).name() : "UIPanel";
        workspaceManager_.getPanelRegistry().registerPanel(
            name, std::move(panel));
    }

    void UIManager::addPanel(const std::string& name, std::unique_ptr<UIPanel> panel) {
        workspaceManager_.getPanelRegistry().registerPanel(name, std::move(panel));
    }

    void UIManager::setToolbarPanel(std::unique_ptr<ToolbarPanel> toolbar) {
        workspaceManager_.setToolbarPanel(std::move(toolbar));
    }

    void UIManager::addToolbarToggle(const std::string& label, UIPanel* panel) {
        workspaceManager_.addToolbarToggle(label, panel);
    }

    void UIManager::render(FrameInfo& frameInfo, VkCommandBuffer commandBuffer) {
        render(frameInfo, commandBuffer, true);
    }

    void UIManager::render(FrameInfo& frameInfo, VkCommandBuffer commandBuffer, bool drawUI) {
        ui::SetActiveWorkspace(&workspaceManager_);
        workspaceManager_.render(frameInfo, commandBuffer, drawUI);
    }

}  // namespace engine
