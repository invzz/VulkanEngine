#include "Editor/ui/UIManager.hpp"

#include <imgui.h>

#include <memory>
#include <utility>

#include "Editor/ui/UIPanel.hpp"
#include "Engine/Graphics/FrameInfo.hpp"
#include "Engine/Graphics/ImGuiManager.hpp"
#include "vulkan/vulkan_core.h"

namespace engine {

UIManager::UIManager(ImGuiManager& imguiManager) : imguiManager_(imguiManager) {}

void UIManager::addPanel(std::unique_ptr<UIPanel> panel) {
  panels_.push_back(std::move(panel));
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

  if (ImGui::BeginMainMenuBar()) {
    if (ImGui::BeginMenu("File")) {
      if (ImGui::MenuItem("Save Scene", "Ctrl+S")) {
        if (onSaveScene_) onSaveScene_();
      }
      if (ImGui::MenuItem("Load Scene", "Ctrl+O")) {
        if (onLoadScene_) onLoadScene_();
      }
      ImGui::EndMenu();
    }
    ImGui::EndMainMenuBar();
  }

  // Create fullscreen dockspace
  ImGuiViewport const* viewport = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(viewport->WorkPos);
  ImGui::SetNextWindowSize(viewport->WorkSize);
  ImGui::SetNextWindowViewport(viewport->ID);

  ImGuiWindowFlags dockspace_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse;
  dockspace_flags |= ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
  dockspace_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
  dockspace_flags |= ImGuiWindowFlags_NoBackground;

  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 8.0f));

  ImGui::Begin("DockSpace", nullptr, dockspace_flags);
  ImGui::PopStyleVar(3);

  // Create dockspace
  ImGuiID const dockspace_id = ImGui::GetID("MainDockSpace");
  ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);
  ImGui::End();

  // Main engine controls window
  ImGui::Begin("Engine Controls");
  ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
  ImGui::Text("Tip: Drag panel headers to dock as sidebars");
  ImGui::Separator();

  // Render all dockable panels (isSeparateWindow=false)
  for (auto& panel : panels_) {
    if (panel->isVisible() && !panel->isSeparateWindow()) {
      panel->render(frameInfo);
    }
  }

  ImGui::End();

  // Render separate window panels (dockable via drag)
  for (auto& panel : panels_) {
    if (panel->isVisible() && panel->isSeparateWindow()) {
      panel->render(frameInfo);
    }
  }

  // Render ImGui
  engine::ImGuiManager::render(commandBuffer);
}
}  // namespace engine
