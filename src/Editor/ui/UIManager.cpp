#include "Editor/ui/UIManager.hpp"

#include <imgui.h>

#include <memory>
#include <utility>

#include "Editor/ui/ToolbarPanel.hpp"
#include "Editor/ui/UIPanel.hpp"
#include "Engine/Graphics/FrameInfo.hpp"
#include "Engine/Graphics/ImGuiManager.hpp"
#include "vulkan/vulkan_core.h"

namespace engine {

UIManager::UIManager(ImGuiManager& imguiManager) : imguiManager_(imguiManager) {}

void UIManager::addPanel(std::unique_ptr<UIPanel> panel) {
  panels_.push_back(std::move(panel));
}

void UIManager::setToolbarPanel(std::unique_ptr<ToolbarPanel> toolbar) {
  toolbarPanel_ = std::move(toolbar);
}

void UIManager::addToolbarToggle(const std::string& label, UIPanel* panel) {
  if (toolbarPanel_) {
    toolbarPanel_->addToggle(label, panel);
  }
}

void UIManager::render(FrameInfo& frameInfo, VkCommandBuffer commandBuffer) {
  render(frameInfo, commandBuffer, true);
}

void UIManager::render(FrameInfo& frameInfo, VkCommandBuffer commandBuffer, bool drawUI) {
  engine::ImGuiManager::newFrame();

  if (!drawUI) {
    // Keep ImGui/Vulkan backend hot so toggling UI (ESC) doesn't hitch.
    // We intentionally build no windows here.
    engine::ImGuiManager::render(commandBuffer);
    return;
  }

  // --- Toolbar (rendered first so it appears on top) ---
  if (toolbarPanel_ && toolbarPanel_->isVisible()) {
    toolbarPanel_->render(frameInfo);
  }

  // --- Main dockspace ---
  ImGuiViewport const* viewport = ImGui::GetMainViewport();
  float                toolbarH = (toolbarPanel_ && toolbarPanel_->isVisible()) ? 32.0f : 0.0f;
  ImVec2               dockPos  = ImVec2(viewport->WorkPos.x, viewport->WorkPos.y + toolbarH);
  ImVec2               dockSize = ImVec2(viewport->WorkSize.x, viewport->WorkSize.y - toolbarH);

  ImGui::SetNextWindowPos(dockPos);
  ImGui::SetNextWindowSize(dockSize);
  ImGui::SetNextWindowViewport(viewport->ID);

  ImGuiWindowFlags dockspace_flags =
      ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
      ImGuiWindowFlags_NoResize   | ImGuiWindowFlags_NoMove |
      ImGuiWindowFlags_NoNav      | ImGuiWindowFlags_NoBackground;

  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 8.0f));

  ImGui::Begin("DockSpace", nullptr, dockspace_flags);
  ImGui::PopStyleVar(3);

  ImGuiID const dockspace_id = ImGui::GetID("MainDockSpace");
  ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);
  ImGui::End();

  // --- Render all visible panels as standalone windows ---
  for (auto& panel : panels_) {
    if (panel->isVisible()) {
      panel->render(frameInfo);
    }
  }

  // --- Render ImGui ---
  engine::ImGuiManager::render(commandBuffer);
}
}  // namespace engine
