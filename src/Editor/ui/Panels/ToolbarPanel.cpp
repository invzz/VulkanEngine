#include "Editor/ui/Panels/ToolbarPanel.hpp"

#include <imgui.h>

#include <ImGuizmo.h>

#include <algorithm>
#include <cstdio>

#include "Editor/ui/UI.hpp"
#include "IconsFontAwesome6.h"
namespace engine {
    ToolbarPanel::ToolbarPanel() = default;
    float ToolbarPanel::getPreferredHeight(float viewportWidth) const {
        bool              compact   = viewportWidth < 1440.0f;
        const ImGuiStyle& style     = ImGui::GetStyle();
        float             rowHeight = ImGui::GetFontSize() + (style.FramePadding.y * 2.0f);
        float             padY      = compact ? 5.0f : 6.0f;
        return std::max(34.0f, rowHeight + (padY * 2.0f) + 2.0f);
    }
    void ToolbarPanel::addToggle(const std::string& label, UIPanel* panel) {
        toggles_.push_back({label, panel});
    }
    void ToolbarPanel::render(FrameInfo& frameInfo) {
        if (!visible_) {
            return;
        }
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImVec2         barPos{viewport->WorkPos.x, viewport->Pos.y};
        bool           compact   = viewport->WorkSize.x < 1440.0f;
        float          barHeight = getPreferredHeight(viewport->WorkSize.x);
        ImGui::SetNextWindowPos(barPos);
        ImGui::SetNextWindowSize(ImVec2(viewport->WorkSize.x, barHeight));
        ImGui::SetNextWindowViewport(viewport->ID);
        ImGuiWindowFlags flags =
            ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoScrollWithMouse |
            ImGuiWindowFlags_NoNav |
            ImGuiWindowFlags_NoFocusOnAppearing |
            ImGuiWindowFlags_NoBringToFrontOnFocus;
        ImVec4 bg           = ImGui::GetStyleColorVec4(ImGuiCol_WindowBg);
        ImVec4 border       = ImGui::GetStyleColorVec4(ImGuiCol_Border);
        ImVec4 textDisabled = ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled);
        ImVec4 accent       = ui::UI::GetAccentColor();
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, compact ? ImVec2(8.0f, 5.0f) : ImVec2(10.0f, 6.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, compact ? ImVec2(4.0f, 4.0f) : ImVec2(6.0f, 6.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, compact ? ImVec2(4.0f, 3.0f) : ImVec2(6.0f, 4.0f));
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(bg.x, bg.y, bg.z, 0.96f));
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(border.x, border.y, border.z, 0.8f));
        ImGui::Begin("Toolbar", nullptr, flags);
        float rowHeight          = ImGui::GetFrameHeight();
        auto  drawSectionDivider = [&](float extraPad = 8.0f) {
            ImGui::SameLine(0.0f, extraPad);
            ImVec2 p = ImGui::GetCursorScreenPos();
            ImGui::GetWindowDrawList()->AddLine(
                ImVec2(p.x, p.y - 2.0f), ImVec2(p.x, p.y + rowHeight + 2.0f),
                ImGui::GetColorU32(ImVec4(border.x, border.y, border.z, 0.55f)), 1.0f);
            ImGui::Dummy(ImVec2(2.0f, rowHeight));
            ImGui::SameLine(0.0f, extraPad);
        };
        auto showTooltip = [&](const char* text) {
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_Stationary)) {
                ImGui::SetTooltip("%s", text);
            }
        };
        auto panelChip = [&](const std::string& label, UIPanel* panel) {
            if (panel == nullptr) {
                return;
            }
            bool   isActive = panel->isVisible();
            ImVec4 chip     = isActive ? ImVec4(accent.x, accent.y, accent.z, 0.28f) : ImVec4(textDisabled.x, textDisabled.y, textDisabled.z, 0.12f);
            ImVec4 chipH    = isActive ? ImVec4(accent.x, accent.y, accent.z, 0.40f) : ImVec4(textDisabled.x, textDisabled.y, textDisabled.z, 0.20f);
            ImVec4 chipA    = isActive ? ImVec4(accent.x, accent.y, accent.z, 0.50f) : ImVec4(textDisabled.x, textDisabled.y, textDisabled.z, 0.28f);
            ImVec4 txt      = isActive ? ui::UI::GetTextColor() : ImVec4(textDisabled.x, textDisabled.y, textDisabled.z, 0.95f);
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
            std::string tip = std::string(isActive ? "Hide " : "Show ") + label + " panel";
            showTooltip(tip.c_str());
        };
        ImGui::TextColored(ImVec4(accent.x, accent.y, accent.z, 1.0f), "%s", ICON_FA_CUBES_STACKED);
        showTooltip("VulkanEngine Editor");
        ImGui::SameLine(0.0f, 6.0f);
        ImGui::AlignTextToFramePadding();
        ImGui::Text("VulkanEngine");
        if (!compact) {
            ImGui::SameLine(0.0f, 6.0f);
            ImGui::TextColored(textDisabled, "EDITOR");
        }
        drawSectionDivider(compact ? 4.0f : 6.0f);
        for (const auto& toggle : toggles_) {
            panelChip(toggle.label, toggle.panel);
            ImGui::SameLine(0.0f, compact ? 3.0f : 4.0f);
        }
        drawSectionDivider(compact ? 4.0f : 6.0f);
        float spacing = compact ? 2.0f : 3.0f;
        if (onResetLayout_) {
            ImGui::SameLine(0.0f, spacing);
            if (ui::UI::ToolbarIcon(ICON_FA_ROTATE_LEFT, false, "layout_reset")) {
                onResetLayout_();
            }
            showTooltip("Reset docking layout");
        }
        if (settingsPanel_ != nullptr) {
            ImGui::SameLine(0.0f, spacing);
            bool active = settingsPanel_->isVisible();
            if (ui::UI::ToolbarIcon(ICON_FA_GEAR, active, "open_settings")) {
                settingsPanel_->setVisible(true);
            }
            showTooltip("Open Settings window");
        }
        ImGui::End();
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(5);
    }
}  // namespace engine
