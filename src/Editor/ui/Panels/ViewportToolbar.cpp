#include "Editor/ui/Panels/ViewportToolbar.hpp"

#include <imgui.h>

#include <ImGuizmo.h>

#include <cstdio>

#include "Engine/Graphics/FrameInfo.hpp"

#include "Editor/ui/UI.hpp"
#include "IconsFontAwesome6.h"
namespace {
    void renderFpsBadge(const engine::FrameInfo& frameInfo, const ImVec2& topLeft, const ImVec2& size) {
        float  fps       = (frameInfo.frameTime > 0.0f) ? (1.0f / frameInfo.frameTime) : 0.0f;
        float  frameMs   = frameInfo.frameTime * 1000.0f;
        ImVec4 windowBg  = ImGui::GetStyleColorVec4(ImGuiCol_WindowBg);
        ImVec4 border    = engine::ui::UI::GetBorderColor();
        ImVec4 textColor = engine::ui::UI::GetTextColor();
        char   perfText[96];
        std::snprintf(
            perfText,
            sizeof(perfText),
            "%s %.1f ms   %s %.0f FPS",
            ICON_FA_STOPWATCH,
            frameMs,
            ICON_FA_GAUGE_HIGH,
            fps);
        ImVec2          textSize           = ImGui::CalcTextSize(perfText);
        constexpr float kToolbarChipHeight = 36.0f;
        constexpr float kToolbarChipWidth  = 180.0f;
        ImVec2          badgeSize(kToolbarChipWidth, kToolbarChipHeight);
        ImVec2          badgeMin(topLeft.x + size.x - badgeSize.x - 10.0f, topLeft.y + 10.0f);
        ImVec2          badgeMax(badgeMin.x + badgeSize.x, badgeMin.y + badgeSize.y);
        ImVec4          badgeBg     = ImVec4(windowBg.x, windowBg.y, windowBg.z, 0.92f);
        ImVec4          badgeBorder = ImVec4(border.x, border.y, border.z, 0.85f);
        ImDrawList*     fg          = ImGui::GetForegroundDrawList();
        fg->AddRectFilled(badgeMin, badgeMax, ImGui::GetColorU32(badgeBg), 8.0f);
        fg->AddRect(badgeMin, badgeMax, ImGui::GetColorU32(badgeBorder), 8.0f);
        float textY = badgeMin.y + ((badgeSize.y - textSize.y) * 0.5f);
        fg->AddText(ImVec2(badgeMin.x + 8.0f, textY), ImGui::GetColorU32(textColor), perfText);
    }
}  // namespace
namespace engine {
    bool ViewportToolbar::render(FrameInfo& frameInfo, const ImVec2& topLeft, const ImVec2& viewportSize) {
        ImGui::SetCursorScreenPos(ImVec2(topLeft.x + 10.0f, topLeft.y + 10.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.06f, 0.08f, 0.12f, 0.85f));
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.40f, 0.45f, 0.55f, 0.45f));
        ImGuiWindowFlags toolbarFlags =
            ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoScrollWithMouse |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoNav |
            ImGuiWindowFlags_NoSavedSettings;
        ImGui::BeginChild("ViewportTools", ImVec2(310.0f, 36.0f), ImGuiChildFlags_Borders, toolbarFlags);
        auto showTooltip = [](const char* text) {
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_Stationary)) {
                ImGui::SetTooltip("%s", text);
            }
        };
        auto gOp          = static_cast<ImGuizmo::OPERATION>(frameInfo.gizmoOperation);
        bool gizmoEnabled = frameInfo.gizmoEnabled;
        auto gOpButton    = [&](const char* icon, int mode, bool active, const char* suffix, const char* tooltip) {
            bool clicked = ui::UI::ToolbarIcon(icon, active, suffix);
            if (clicked && gizmoEnabled) {
                frameInfo.gizmoOperation = (gOp == mode) ? ImGuizmo::TRANSLATE : mode;
            }
            showTooltip(tooltip);
        };
        gOpButton(ICON_FA_UP_DOWN_LEFT_RIGHT, ImGuizmo::TRANSLATE,
            (gOp & ImGuizmo::TRANSLATE) == ImGuizmo::TRANSLATE, "vp_translate", "Translate (T)");
        ImGui::SameLine(0.0f, 3.0f);
        gOpButton(ICON_FA_ROTATE, ImGuizmo::ROTATE,
            (gOp & ImGuizmo::ROTATE) == ImGuizmo::ROTATE, "vp_rotate", "Rotate (R)");
        ImGui::SameLine(0.0f, 3.0f);
        gOpButton(ICON_FA_ARROWS_UP_DOWN, ImGuizmo::SCALE,
            (gOp & ImGuizmo::SCALE) == ImGuizmo::SCALE, "vp_scale", "Scale (S)");
        ImGui::SameLine(0.0f, 6.0f);
        bool isWorld = (frameInfo.gizmoMode == 1);
        if (ui::UI::ToolbarIcon(ICON_FA_GLOBE, isWorld, "vp_space")) {
            if (gizmoEnabled) {
                frameInfo.gizmoMode = isWorld ? 0 : 1;
            }
        }
        showTooltip("Toggle world/local space (W)");
        ImGui::SameLine(0.0f, 3.0f);
        if (ui::UI::ToolbarIcon(ICON_FA_CROSSHAIRS, frameInfo.gizmoEnabled, "vp_gizmo_toggle")) {
            frameInfo.gizmoEnabled = !frameInfo.gizmoEnabled;
        }
        showTooltip("Enable/disable gizmo");
        ImGui::SameLine(0.0f, 6.0f);
        bool nav = (frameInfo.viewportMode == ViewportMode::Navigation);
        if (ui::UI::ToolbarIcon(ICON_FA_COMPASS, nav, "vp_mode")) {
            frameInfo.viewportMode = nav ? ViewportMode::Picking : ViewportMode::Navigation;
        }
        showTooltip(nav ? "Viewport mode: Navigation" : "Viewport mode: Picking");
        ImGui::SameLine(0.0f, 6.0f);
        if (ImGui::SmallButton(frameInfo.viewGizmoOrbitSelected ? "Orbit Sel" : "Look In Place")) {
            frameInfo.viewGizmoOrbitSelected = !frameInfo.viewGizmoOrbitSelected;
        }
        showTooltip(frameInfo.viewGizmoOrbitSelected
                        ? "View gizmo: Orbit selected pivot"
                        : "View gizmo: Reorient camera in place");
        bool hovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByPopup);
        ImGui::EndChild();
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);
        renderFpsBadge(frameInfo, topLeft, viewportSize);
        return hovered;
    }
}  // namespace engine
