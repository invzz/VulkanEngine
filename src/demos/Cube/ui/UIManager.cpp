#include "UIManager.hpp"

#include <imgui.h>

namespace engine {

  UIManager::UIManager(ImGuiManager& imguiManager) : imguiManager_(imguiManager) {}

  void UIManager::addPanel(std::unique_ptr<UIPanel> panel)
  {
    panels_.push_back(std::move(panel));
  }

  void UIManager::render(FrameInfo& frameInfo, VkCommandBuffer commandBuffer)
  {
    imguiManager_.newFrame();

    if (ImGui::BeginMainMenuBar())
    {
      if (ImGui::BeginMenu("File"))
      {
        if (ImGui::MenuItem("Save Scene", "Ctrl+S"))
        {
          if (onSaveScene_) onSaveScene_();
        }
        if (ImGui::MenuItem("Load Scene", "Ctrl+O"))
        {
          if (onLoadScene_) onLoadScene_();
        }
        ImGui::EndMenu();
      }
      ImGui::EndMainMenuBar();
    }

    // Create fullscreen dockspace
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);

    ImGuiWindowFlags dockspace_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse;
    dockspace_flags |= ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
    dockspace_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
    dockspace_flags |= ImGuiWindowFlags_NoBackground;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

    ImGui::Begin("DockSpace", nullptr, dockspace_flags);
    ImGui::PopStyleVar(3);

    // Create dockspace
    ImGuiID dockspace_id = ImGui::GetID("MainDockSpace");
    ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);
    ImGui::End();

    // Main engine controls window (now dockable)
    ImGui::Begin("Engine Controls");
    ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
    ImGui::Text("Tip: Drag panel headers to dock as sidebars");
    ImGui::Separator();

    // Render all panels
    for (auto& panel : panels_)
    {
      if (panel->isVisible())
      {
        panel->render(frameInfo);
      }
    }

    ImGui::End();

    // Render ImGui
    imguiManager_.render(commandBuffer);
  }
} // namespace engine
