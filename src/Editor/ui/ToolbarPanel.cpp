#include "Editor/ui/ToolbarPanel.hpp"

#include "IconsFontAwesome6.h"
#include "Editor/ui/UI.hpp"
#include <imgui.h>

#include <ImGuizmo.h>

#include <algorithm>
#include <cmath>

namespace engine {

    ToolbarPanel::ToolbarPanel() = default;

    void ToolbarPanel::addToggle(const std::string& label, UIPanel* panel) {
        toggles_.push_back({label, panel});
    }

    void ToolbarPanel::render(FrameInfo& frameInfo) {
        if (!visible_)
            return;

        // Always render the toolbar at the very top of the viewport
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImVec2         barPos{viewport->WorkPos.x, viewport->Pos.y};
        float          barHeight = 64.0f;

        ImGui::SetNextWindowPos(barPos);
        ImGui::SetNextWindowSize(ImVec2(viewport->WorkSize.x, barHeight));
        ImGui::SetNextWindowViewport(viewport->ID);

        // Toolbar window flags: no border, no collapse, no background, pass-thru
        ImGuiWindowFlags flags =
            ImGuiWindowFlags_NoTitleBar |
          
            ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoScrollWithMouse |
            ImGuiWindowFlags_NoNav ;
            
      


        ImGui::Begin("Toolbar", nullptr, flags);

        // --- Left: Application name ---
        ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f), "VulkanEngine");

        // --- Panel toggles ---
        for (const auto& toggle : toggles_) {
            bool isActive = toggle.panel != nullptr && toggle.panel->isVisible();
            if (ImGui::Checkbox(toggle.label.c_str(), &isActive)) {
                if (toggle.panel) {
                    toggle.panel->setVisible(isActive);
                }
            }
            ImGui::SameLine(0.0f, 0.0f);
        }

        // --- Gizmo operation buttons (T / R / S) ---
        ImGui::SameLine(0.0f, 16.0f);
        ImGui::Text("%s Gizmo:", ICON_FA_VECTOR_SQUARE);
        ImGui::SameLine(0.0f, 0.0f);

        auto gOp = static_cast<ImGuizmo::OPERATION>(frameInfo.gizmoOperation);
        auto isEnabled = frameInfo.gizmoEnabled;

        bool tOp = (gOp & ImGuizmo::TRANSLATE) == ImGuizmo::TRANSLATE;
        bool rOp = (gOp & ImGuizmo::ROTATE) == ImGuizmo::ROTATE;
        bool sOp = (gOp & ImGuizmo::SCALE) == ImGuizmo::SCALE;

        ImGui::BeginDisabled(!isEnabled);
        if (ui::UI::ToolbarIcon(ICON_FA_UP_DOWN_LEFT_RIGHT, tOp, "gizmo_translate")) {
            frameInfo.gizmoOperation = ImGuizmo::TRANSLATE;
        }
        ImGui::SameLine(0.0f, 2.0f);
        if (ui::UI::ToolbarIcon(ICON_FA_ROTATE, rOp, "gizmo_rotate")) {
            frameInfo.gizmoOperation = ImGuizmo::ROTATE;
        }
        ImGui::SameLine(0.0f, 2.0f);
        if (ui::UI::ToolbarIcon(ICON_FA_ARROWS_UP_DOWN, sOp, "gizmo_scale")) {
            frameInfo.gizmoOperation = ImGuizmo::SCALE;
        }
        ImGui::EndDisabled();

        // --- World / Local space toggle ---
        ImGui::SameLine(0.0f, 8.0f);
        bool isWorld = (frameInfo.gizmoMode == 1);
        if (ui::UI::ToolbarIcon(ICON_FA_GLOBE, isEnabled && isWorld, "gizmo_space")) {
            if (isEnabled) frameInfo.gizmoMode = isWorld ? 0 : 1;
        }

        // --- Gizmo enable toggle ---
        ImGui::SameLine(0.0f, 8.0f);
        if (ui::UI::ToolbarIcon(ICON_FA_CROSSHAIRS, frameInfo.gizmoEnabled, "gizmo_enable")) {
            frameInfo.gizmoEnabled = !frameInfo.gizmoEnabled;
        }

        // --- Viewport mode toggle ---
        ImGui::SameLine(0.0f, 16.0f);
        bool nav = (frameInfo.viewportMode == ViewportMode::Navigation);
        if (ui::UI::ToolbarIcon(ICON_FA_COMPASS, nav, "nav_mode")) {
            frameInfo.viewportMode = nav ? ViewportMode::Picking : ViewportMode::Navigation;
        }

        // --- Layout reset ---
        if (onResetLayout_) {
            ImGui::SameLine(0.0f, 16.0f);
            if (ImGui::Button((std::string(ICON_FA_ROTATE_LEFT) + " Reset Layout").c_str())) {
                onResetLayout_();
            }
        }

        // --- Right: Metrics ---
        float fps       = ImGui::GetIO().Framerate;
        float frameTime = frameTimeMs_ > 0.0f ? frameTimeMs_ : (1000.0f / std::max(fps, 1.0f));

        // Color-code frame time
        ImVec4 frameColor;
        if (frameTime < 16.67f) {
            frameColor = ImVec4(0.3f, 1.0f, 0.3f, 1.0f);  // green: <60fps
        } else if (frameTime < 33.33f) {
            frameColor = ImVec4(1.0f, 0.85f, 0.0f, 1.0f);  // yellow: 30-60fps
        } else {
            frameColor = ImVec4(1.0f, 0.3f, 0.3f, 1.0f);  // red: <30fps
        }

        ImGui::TextColored(frameColor, "%.1f ms", frameTime);
        ImGui::SameLine(0.0f, 0.0f);
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "  %.0f FPS", fps);

        ImGui::End();
    }}