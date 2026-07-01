#include "Editor/ui/ToolbarPanel.hpp"

#include "IconsFontAwesome6.h"
#include <imgui.h>

#include <ImGuizmo.h>

#include "Editor/ui/UI.hpp"
#include <algorithm>
#include <cstdio>

namespace engine {

    ToolbarPanel::ToolbarPanel() = default;

    void ToolbarPanel::addToggle(const std::string& label, UIPanel* panel) {
        toggles_.push_back({label, panel});
    }

    void ToolbarPanel::render(FrameInfo& frameInfo) {
        if (!visible_) {
            return;
        }

        // Always render the toolbar at the very top of the viewport
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImVec2         barPos{viewport->WorkPos.x, viewport->Pos.y};
        float          barHeight = 52.0f;

        ImGui::SetNextWindowPos(barPos);
        ImGui::SetNextWindowSize(ImVec2(viewport->WorkSize.x, barHeight));
        ImGui::SetNextWindowViewport(viewport->ID);

        // Toolbar window flags: fixed thin top strip
        ImGuiWindowFlags flags =
            ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoScrollWithMouse |
            ImGuiWindowFlags_NoNav |
            ImGuiWindowFlags_NoFocusOnAppearing |
            ImGuiWindowFlags_NoBringToFrontOnFocus;

        ImVec4 bg = ImGui::GetStyleColorVec4(ImGuiCol_WindowBg);
        ImVec4 border = ImGui::GetStyleColorVec4(ImGuiCol_Border);
        ImVec4 textDisabled = ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled);
        ImVec4 accent = ui::UI::GetAccentColor();

        // Render toolbar as a polished top strip
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0f, 8.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6.0f, 6.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, ImVec2(6.0f, 4.0f));
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(bg.x, bg.y, bg.z, 0.96f));
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(border.x, border.y, border.z, 0.8f));

        ImGui::Begin("Toolbar", nullptr, flags);

        float rowHeight = ImGui::GetFrameHeight();

        auto drawSectionDivider = [&](float extraPad = 8.0f) {
            ImGui::SameLine(0.0f, extraPad);
            ImVec2 p = ImGui::GetCursorScreenPos();
            ImGui::GetWindowDrawList()->AddLine(
                ImVec2(p.x, p.y - 2.0f), ImVec2(p.x, p.y + rowHeight + 2.0f),
                ImGui::GetColorU32(ImVec4(border.x, border.y, border.z, 0.55f)), 1.0f);
            ImGui::Dummy(ImVec2(2.0f, rowHeight));
            ImGui::SameLine(0.0f, extraPad);
        };

        auto panelChip = [&](const std::string& label, UIPanel* panel) {
            if (panel == nullptr) {
                return;
            }
            bool isActive = panel->isVisible();
            ImVec4 chip = isActive ? ImVec4(accent.x, accent.y, accent.z, 0.28f) : ImVec4(textDisabled.x, textDisabled.y, textDisabled.z, 0.12f);
            ImVec4 chipH = isActive ? ImVec4(accent.x, accent.y, accent.z, 0.40f) : ImVec4(textDisabled.x, textDisabled.y, textDisabled.z, 0.20f);
            ImVec4 chipA = isActive ? ImVec4(accent.x, accent.y, accent.z, 0.50f) : ImVec4(textDisabled.x, textDisabled.y, textDisabled.z, 0.28f);
            ImVec4 txt = isActive ? ui::UI::GetTextColor() : ImVec4(textDisabled.x, textDisabled.y, textDisabled.z, 0.95f);

            ImGui::PushStyleColor(ImGuiCol_Button, chip);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, chipH);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, chipA);
            ImGui::PushStyleColor(ImGuiCol_Text, txt);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 7.0f);
            bool clicked = ImGui::Button(label.c_str(), ImVec2(0.0f, rowHeight));
            ImGui::PopStyleVar();
            ImGui::PopStyleColor(4);

            if (clicked) {
                panel->setVisible(!isActive);
            }
        };

        // Left brand
        ImGui::TextColored(ImVec4(accent.x, accent.y, accent.z, 1.0f), "%s", ICON_FA_CUBES_STACKED);
        ImGui::SameLine(0.0f, 6.0f);
        ImGui::AlignTextToFramePadding();
        ImGui::Text("VulkanEngine");
        ImGui::SameLine(0.0f, 6.0f);
        ImGui::TextColored(textDisabled, "EDITOR");

        drawSectionDivider();

        // Panel toggles as chips
        for (const auto& toggle : toggles_) {
            panelChip(toggle.label, toggle.panel);
            ImGui::SameLine(0.0f, 6.0f);
        }

        drawSectionDivider();

        // Gizmo tools
        ImGui::AlignTextToFramePadding();
        ImGui::TextColored(textDisabled, "Gizmo");
        ImGui::SameLine(0.0f, 6.0f);

        auto gOp = static_cast<ImGuizmo::OPERATION>(frameInfo.gizmoOperation);
        auto isEnabled = frameInfo.gizmoEnabled;

        auto gOpButton = [&](const char* icon, int mode, bool active, const char* suffix) {
            bool clicked = ui::UI::ToolbarIcon(icon, active, suffix);
            if (clicked && isEnabled) {
                frameInfo.gizmoOperation = (gOp == mode) ? ImGuizmo::TRANSLATE : mode;
            }
        };

        float spacing = 4.0f;

        gOpButton(ICON_FA_UP_DOWN_LEFT_RIGHT, ImGuizmo::TRANSLATE, (gOp & ImGuizmo::TRANSLATE) == ImGuizmo::TRANSLATE, "translate");
        ImGui::SameLine(0.0f, spacing);
        gOpButton(ICON_FA_ROTATE, ImGuizmo::ROTATE, (gOp & ImGuizmo::ROTATE) == ImGuizmo::ROTATE, "rotate");
        ImGui::SameLine(0.0f, spacing);
        gOpButton(ICON_FA_ARROWS_UP_DOWN, ImGuizmo::SCALE, (gOp & ImGuizmo::SCALE) == ImGuizmo::SCALE, "scale");

        // --- World / Local space toggle ---
        ImGui::SameLine(0.0f, spacing);

        bool isWorld = (frameInfo.gizmoMode == 1);
        {
            bool active = isWorld;
            bool clicked = ui::UI::ToolbarIcon(ICON_FA_GLOBE, active, "space");
            if (clicked && isEnabled) {
                frameInfo.gizmoMode = isWorld ? 0 : 1;
            }
        }

        // --- Gizmo enable toggle ---
        ImGui::SameLine(0.0f, spacing);

        {
            bool active = frameInfo.gizmoEnabled;
            bool clicked = ui::UI::ToolbarIcon(ICON_FA_CROSSHAIRS, active, "enable");
            if (clicked) {
                frameInfo.gizmoEnabled = !frameInfo.gizmoEnabled;
            }
        }

        // --- Viewport mode toggle ---
        ImGui::SameLine(0.0f, spacing);
        {
            bool nav = (frameInfo.viewportMode == ViewportMode::Navigation);
            bool active = nav;
            bool clicked = ui::UI::ToolbarIcon(ICON_FA_COMPASS, active, "viewport_mode");

            if (clicked) {
                frameInfo.viewportMode = nav ? ViewportMode::Picking : ViewportMode::Navigation;
            }
        }

        // Layout reset
        if (onResetLayout_) {
            ImGui::SameLine(0.0f, spacing);
            if (ui::UI::ToolbarIcon(ICON_FA_ROTATE_LEFT, false, "layout_reset")) {
                onResetLayout_();
            }
        }

        // Right-aligned performance metrics
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

        char perfText[64];
        std::snprintf(perfText, sizeof(perfText), "%.1f ms  %.0f FPS", frameTime, fps);

        float perfWidth = ImGui::CalcTextSize(perfText).x + 18.0f;
        float perfX = ImGui::GetWindowContentRegionMax().x - perfWidth;
        if (ImGui::GetCursorPosX() < perfX) {
            ImGui::SetCursorPosX(perfX);
        } else {
            ImGui::SameLine(0.0f, 8.0f);
        }

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(frameColor.x, frameColor.y, frameColor.z, 0.18f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(frameColor.x, frameColor.y, frameColor.z, 0.24f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(frameColor.x, frameColor.y, frameColor.z, 0.24f));
        ImGui::PushStyleColor(ImGuiCol_Text, frameColor);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
        ImGui::Button(perfText, ImVec2(perfWidth, rowHeight));
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(4);

        ImGui::End();
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(5);
    }

    

}  // namespace engine
