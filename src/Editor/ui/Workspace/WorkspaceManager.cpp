#include "Editor/ui/Workspace/WorkspaceManager.hpp"

#include <imgui.h>

#include "Engine/Graphics/ImGuiManager.hpp"

#include "Editor/ui/Panels/ToolbarPanel.hpp"

namespace engine {

    WorkspaceManager::WorkspaceManager() = default;

    void WorkspaceManager::initialize(ImGuiManager& imguiManager) {
        imguiManager_ = &imguiManager;
        // Apply default theme
        themeSystem_.applyTheme("dark");
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
        float                toolbarH = 0.0f;
        if (toolbarVisible_ && toolbarPanel_ && toolbarPanel_->isVisible()) {
            toolbarH = toolbarPanel_->getPreferredHeight(viewport->WorkSize.x);
        }
        ImVec2 dockPos  = ImVec2(viewport->WorkPos.x, viewport->WorkPos.y + toolbarH);
        ImVec2 dockSize = ImVec2(viewport->WorkSize.x, viewport->WorkSize.y - toolbarH);

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

        // First frame (or after a Reset): build the dock tree from the registered
        // panels' DockConstraints. Without this step, every panel falls back to
        // a floating top-level window and docking does not work.
        if (!layoutApplied_) {
            LayoutBuilder builder;
            for (const auto& name : panelRegistry_.getPanelNames()) {
                builder.addEntry(name, panelRegistry_.getConstraints(name).preferredZone);
            }
            builder.apply(mainDockspaceID_, dockSize, currentLayout_);
            layoutApplied_ = true;
        }

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
        layoutApplied_ = false;  // force a rebuild on next frame
    }

    void WorkspaceManager::resetLayout() {
        applyLayoutPreset(currentLayout_);
    }

    void WorkspaceManager::registerPanel(const std::string& name, std::unique_ptr<UIPanel> panel,
        DockConstraints constraints) {
        panelRegistry_.registerPanel(name, std::move(panel), constraints);
    }

    void WorkspaceManager::setToolbarPanel(std::unique_ptr<class ToolbarPanel> toolbar) {
        toolbarPanel_ = std::move(toolbar);
        if (toolbarPanel_) {
            // Wire the toolbar's "Reset Layout" button to our resetLayout().
            toolbarPanel_->setOnResetLayout([this]() { resetLayout(); });
        }
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
        // Sanity invariants we can check without walking the dock tree:
        //  1. The layout has been applied at least once.
        //  2. Every registered panel has a non-empty name (the name is the
        //     ImGui window title, an empty one would silently fail to dock).
        //  3. Every constraint is within bounds (delegated to enforceConstraints).
        if (!layoutApplied_) {
            return false;
        }

        auto& registry = getPanelRegistry();
        auto  names    = registry.getPanelNames();
        if (names.empty()) {
            return false;
        }

        for (const auto& name : names) {
            if (name.empty()) {
                return false;
            }
            if (!enforceConstraints(name, registry.getConstraints(name))) {
                return false;
            }
        }

        return true;
    }

    bool WorkspaceManager::enforceConstraints(const std::string& name, const DockConstraints& constraints) {
        // Currently a pure validator. The ImGui side enforces minSize on the
        // window class; this method exists so callers can pre-check constraints
        // before registration and so validateLayout() has a single place to
        // extend with future rules.
        if (name.empty()) {
            return false;
        }
        if (constraints.minSizeX < 50.0f || constraints.minSizeY < 50.0f) {
            return false;
        }
        if (constraints.minSizeX > 5000.0f || constraints.minSizeY > 5000.0f) {
            return false;
        }
        return true;
    }

}  // namespace engine
