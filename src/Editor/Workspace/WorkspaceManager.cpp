#include "Editor/Workspace/WorkspaceManager.hpp"

#include <imgui.h>
#include <imgui_internal.h>

#include "Engine/Graphics/ImGuiManager.hpp"

#include "Editor/ui/ToolbarPanel.hpp"

namespace engine {

    WorkspaceManager::WorkspaceManager() = default;

    void WorkspaceManager::initialize(ImGuiManager& imguiManager) {
        imguiManager_ = &imguiManager;
        // Apply default theme
        themeSystem_.applyPreset(0);
    }

    void WorkspaceManager::render(FrameInfo& frameInfo, VkCommandBuffer commandBuffer, bool drawUI) {
        engine::ImGuiManager::newFrame();

        if (!drawUI) {
            engine::ImGuiManager::render(commandBuffer);
            return;
        }

        // --- Toolbar ---
        if (toolbarVisible_ && toolbarPanel_ && toolbarPanel_->isVisible()) {
            toolbarPanel_->render(frameInfo);
        }

        // --- Main dockspace ---
        ImGuiViewport const* viewport = ImGui::GetMainViewport();
        float                toolbarH = (toolbarVisible_ && toolbarPanel_ && toolbarPanel_->isVisible()) ? 64.0f : 0.0f;
        ImVec2               dockPos  = ImVec2(viewport->WorkPos.x, viewport->WorkPos.y + toolbarH);
        ImVec2               dockSize = ImVec2(viewport->WorkSize.x, viewport->WorkSize.y - toolbarH);

        ImGui::SetNextWindowPos(dockPos);
        ImGui::SetNextWindowSize(dockSize);
        ImGui::SetNextWindowViewport(viewport->ID);

        ImGuiWindowFlags dockspace_flags =
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoBackground;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 8.0f));

        ImGui::Begin("DockSpace", nullptr, dockspace_flags);
        ImGui::PopStyleVar(3);

        mainDockspaceID_ = ImGui::GetID("MainDockSpace");
        ImGui::DockSpace(mainDockspaceID_, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);
        ImGui::End();

        // --- Render all visible panels ---
        auto& registry = getPanelRegistry();
        for (auto* panel : registry.getAllPanels()) {
            if (panel->isVisible()) {
                panel->render(frameInfo);
            }
        }

        // --- Render ImGui ---
        engine::ImGuiManager::render(commandBuffer);
    }

    void WorkspaceManager::applyLayoutPreset(LayoutPreset preset) {
        currentLayout_ = preset;
    }

    void WorkspaceManager::registerPanel(const std::string& name, std::unique_ptr<UIPanel> panel,
        DockConstraints constraints) {
        panelRegistry_.registerPanel(name, std::move(panel), constraints);
    }

    void WorkspaceManager::setToolbarPanel(std::unique_ptr<class ToolbarPanel> toolbar) {
        toolbarPanel_ = std::move(toolbar);
    }

    void WorkspaceManager::addToolbarToggle(const std::string& label, UIPanel* panel) {
        toolbarToggles_.push_back({label, panel});
    }

    void WorkspaceManager::setFrameTimeMs(float ms) {
        frameTimeMs_ = ms;
        if (toolbarPanel_) {
            toolbarPanel_->setFrameTime(frameTimeMs_);
        }
    }

    bool WorkspaceManager::validateLayout() {
        bool violationsFound = false;

        // Validate all panels against their constraints
        auto& registry = getPanelRegistry();
        auto  panels   = registry.getAllPanels();

        for (auto* panel : panels) {
            // Check if panel is visible and has constraints
            if (!panel->isVisible())
                continue;

            // Rule 1: Toolbar panel must be at top
            // Rule 2: Viewport panel (future) must be center
            // Rule 3: Inspector must be right
            // Rule 4: Scene hierarchy must be left
            // Rule 5: No panel should be floating outside dockspace
        }

        return !violationsFound;
    }

    bool WorkspaceManager::enforceConstraints(const std::string& name, const DockConstraints& constraints) {
        // Validate constraints
        if (constraints.minSizeX < 50.0f || constraints.minSizeY < 50.0f) {
            return false;
        }

        if (constraints.minSizeX > 5000.0f || constraints.minSizeY > 5000.0f) {
            return false;
        }

        return true;
    }

}  // namespace engine
