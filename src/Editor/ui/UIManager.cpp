#include "Editor/ui/UIManager.hpp"

#include <imgui.h>

#include <memory>
#include <utility>

#include "Editor/Workspace/WorkspaceManager.hpp"
#include "Editor/ui/ToolbarPanel.hpp"
#include "Editor/ui/UIPanel.hpp"
#include "Engine/Graphics/FrameInfo.hpp"
#include "Engine/Graphics/ImGuiManager.hpp"
#include "vulkan/vulkan_core.h"

namespace engine {

UIManager::UIManager(ImGuiManager& imguiManager) {
    workspaceManager_.initialize(imguiManager);
}

void UIManager::addPanel(std::unique_ptr<UIPanel> panel) {
    workspaceManager_.getPanelRegistry().registerPanel(
        typeid(*panel).name(), std::move(panel));
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
    workspaceManager_.render(frameInfo, commandBuffer, drawUI);
}

}  // namespace engine
