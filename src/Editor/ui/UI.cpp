#include "Editor/ui/UI.hpp"

#include <imgui.h>

#include <algorithm>
#include <string>

#include "Editor/ui/Workspace/ThemeSystem.hpp"
#include "Editor/ui/Workspace/WorkspaceManager.hpp"

namespace engine::ui {

    // ======================================================================
    // Implementation
    // ======================================================================

    // Get the current WorkspaceManager (set by the active panel context)
    static WorkspaceManager* s_activeWorkspace = nullptr;

    /**
 * @brief Set the active workspace context for UI calls.
 * Call this at the start of each panel's render() method.
 */
    void SetActiveWorkspace(WorkspaceManager* wm) {
        s_activeWorkspace = wm;
    }

    // Helper to get the theme system
    static ThemeSystem& GetTheme() {
        static ThemeSystem fallback;
        if (s_activeWorkspace) {
            return s_activeWorkspace->getThemeSystem();
        }
        return fallback;
    }

    // Helper to get the UI state
    static UIState& GetUIState() {
        static UIState fallback;
        if (s_activeWorkspace) {
            return s_activeWorkspace->getUIState();
        }
        return fallback;
    }

    // ======================================================================
    // Section / Grouping
    // ======================================================================

    bool UI::Section(const char* label, bool* open_ptr) {
        ImVec4 accent = GetTheme().getAccentColor();

        // Draw accent bar
        ImVec2 pos = ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton("##section_space", ImVec2(3.0f, 0.0f));
        ImVec4 borderColor = GetTheme().getBorderColor();
        ImGui::GetWindowDrawList()->AddRectFilled(
            ImVec2(pos.x - 1.0f, pos.y),
            ImVec2(pos.x + 2.0f, pos.y + ImGui::GetTextLineHeight()),
            ImGui::GetColorU32(borderColor),
            1.0f);

        // Render header
        bool open = ImGui::CollapsingHeader(label, open_ptr, 0);
        return open;
    }

    bool UI::Section(const char* label, ImVec4 accent_color, bool* open_ptr) {
        return Section(label, open_ptr);  // Uses accent color from theme
    }

    void UI::Separator() {
        ImVec4 sep = GetTheme().getSeparatorColor();
        ImGui::PushStyleColor(ImGuiCol_Separator, sep);
        ImGui::Separator();
        ImGui::PopStyleColor();
    }

    void UI::Divider() {
        ImVec4 border = GetTheme().getBorderColor();
        ImGui::PushStyleColor(ImGuiCol_Separator, border);
        ImGui::Separator();
        ImGui::PopStyleColor();
    }

    // ======================================================================
    // Buttons
    // ======================================================================

    bool UI::Button(const char* label) {
        ImVec4 btn    = GetTheme().getColor(ImGuiCol_Button);
        ImVec4 btnHov = GetTheme().getColor(ImGuiCol_ButtonHovered);
        ImVec4 btnAct = GetTheme().getColor(ImGuiCol_ButtonActive);
        ImVec4 text   = GetTheme().getTextColor();

        ImGui::PushStyleColor(ImGuiCol_Button, btn);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, btnHov);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, btnAct);
        ImGui::PushStyleColor(ImGuiCol_Text, text);
        bool clicked = ImGui::Button(label);
        ImGui::PopStyleColor(4);
        return clicked;
    }

    bool UI::SmallButton(const char* label) {
        ImVec4 btn    = GetTheme().getColor(ImGuiCol_Button);
        ImVec4 btnHov = GetTheme().getColor(ImGuiCol_ButtonHovered);
        ImVec4 btnAct = GetTheme().getColor(ImGuiCol_ButtonActive);
        ImVec4 text   = GetTheme().getTextColor();

        ImGui::PushStyleColor(ImGuiCol_Button, btn);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, btnHov);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, btnAct);
        ImGui::PushStyleColor(ImGuiCol_Text, text);
        bool clicked = ImGui::SmallButton(label);
        ImGui::PopStyleColor(4);
        return clicked;
    }

    bool UI::IconButton(const char* icon, const char* label) {
        ImGui::Text("%s", icon);
        ImGui::SameLine(0.0f, 2.0f);
        return SmallButton(label);
    }

    bool UI::ToolbarIcon(const char* icon, bool active, const char* suffix) {
        ImVec4 btnColor   = active ? ImVec4(0.15f, 0.15f, 0.18f, 1.0f) : ImVec4(0.08f, 0.08f, 0.10f, 1.0f);
        ImVec4 btnHovColor = active ? ImVec4(0.25f, 0.25f, 0.30f, 1.0f) : ImVec4(0.12f, 0.12f, 0.14f, 1.0f);
        ImVec4 btnActColor = active ? ImVec4(0.35f, 0.35f, 0.40f, 1.0f) : ImVec4(0.15f, 0.15f, 0.18f, 1.0f);
        ImVec4 textColor   = active ? ImVec4(1.0f, 1.0f, 1.0f, 1.0f) : ImVec4(0.7f, 0.7f, 0.75f, 1.0f);

        ImGui::PushStyleColor(ImGuiCol_Button, btnColor);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, btnHovColor);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, btnActColor);
        ImGui::PushStyleColor(ImGuiCol_Text, textColor);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10.0f, 10.0f));
        std::string id = std::string(icon) + "##toolbar_" + suffix;
        bool clicked = ImGui::SmallButton(id.c_str());
        ImGui::PopStyleVar(1);
        ImGui::PopStyleColor(4);
        return clicked;
    }

    bool UI::DisabledButton(const char* label) {
        ImVec4 disabled = GetTheme().getDisabledColor();
        ImVec4 text     = GetTheme().getTextColor();

        ImGui::PushStyleColor(ImGuiCol_Text, disabled);
        bool clicked = SmallButton(label);
        ImGui::PopStyleColor();
        return clicked;
    }

    // ======================================================================
    // Properties
    // ======================================================================

    void UI::Property(const char* label, const char* value) {
        ImVec4 disabled = GetTheme().getDisabledColor();
        ImVec4 text     = GetTheme().getTextColor();

        ImGui::TextColored(disabled, "%s:", label);
        ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - ImGui::CalcTextSize(value).x - 10.0f);
        ImGui::TextColored(text, "%s", value);
    }

    void UI::Property(const char* label, float value, ImVec4 dot_color) {
        ImVec4 disabled = GetTheme().getDisabledColor();
        ImVec4 text     = GetTheme().getTextColor();

        ImGui::TextColored(disabled, "%s:", label);
        ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - ImGui::CalcTextSize("999.99").x - 10.0f);
        if (dot_color.w > 0.0f) {
            ImVec2 pos = ImGui::GetCursorScreenPos();
            ImGui::GetWindowDrawList()->AddCircleFilled(
                ImVec2(pos.x - 5.0f, pos.y + ImGui::GetTextLineHeightWithSpacing() / 2.0f),
                3.0f, ImGui::GetColorU32(dot_color));
        }
        ImGui::TextColored(text, "%.2f", value);
    }

    bool UI::PropertyBool(const char* label, bool* value) {
        ImVec4 disabled = GetTheme().getDisabledColor();
        ImVec4 text     = GetTheme().getTextColor();

        ImGui::TextColored(disabled, "%s:", label);
        ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - 80.0f);
        bool changed = ImGui::Checkbox("", value);
        return changed;
    }

    // ======================================================================
    // Tree / Hierarchy
    // ======================================================================

    bool UI::TreeNode(const char* label, ImGuiTreeNodeFlags flags) {
        ImVec4 header    = GetTheme().getColor(ImGuiCol_Header);
        ImVec4 headerHov = GetTheme().getColor(ImGuiCol_HeaderHovered);
        ImVec4 headerAct = GetTheme().getColor(ImGuiCol_HeaderActive);
        ImVec4 border    = GetTheme().getBorderColor();

        ImGui::PushStyleColor(ImGuiCol_Header, header);
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, headerHov);
        ImGui::PushStyleColor(ImGuiCol_HeaderActive, headerAct);
        ImGui::PushStyleColor(ImGuiCol_Border, border);
        bool open = ImGui::TreeNodeEx(label, flags);
        ImGui::PopStyleColor(4);
        return open;
    }

    bool UI::TreeNode(const char* icon, const char* label, ImGuiTreeNodeFlags flags) {
        ImGui::Text("%s", icon);
        ImGui::SameLine(0.0f, 2.0f);
        return TreeNode(label, flags);
    }

    bool UI::Selectable(const char* label, bool selected, ImGuiSelectableFlags flags) {
        ImVec4 header    = GetTheme().getColor(ImGuiCol_Header);
        ImVec4 headerHov = GetTheme().getColor(ImGuiCol_HeaderHovered);
        ImVec4 headerAct = GetTheme().getColor(ImGuiCol_HeaderActive);
        ImVec4 text      = GetTheme().getTextColor();

        ImGui::PushStyleColor(ImGuiCol_Header, header);
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, headerHov);
        ImGui::PushStyleColor(ImGuiCol_HeaderActive, headerAct);
        ImGui::PushStyleColor(ImGuiCol_Text, selected ? ImVec4(1.0f, 1.0f, 1.0f, 1.0f) : text);
        bool clicked = ImGui::Selectable(label, selected, flags);
        ImGui::PopStyleColor(4);
        return clicked;
    }

    // ======================================================================
    // Input Controls
    // ======================================================================

    bool UI::InputText(const char* label, char* buffer, int buf_size) {
        ImVec4 frameBg    = GetTheme().getColor(ImGuiCol_FrameBg);
        ImVec4 frameBgHov = GetTheme().getColor(ImGuiCol_FrameBgHovered);
        ImVec4 frameBgAct = GetTheme().getColor(ImGuiCol_FrameBgActive);
        ImVec4 border     = GetTheme().getBorderColor();
        ImVec4 text       = GetTheme().getTextColor();

        ImGui::PushStyleColor(ImGuiCol_FrameBg, frameBg);
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, frameBgHov);
        ImGui::PushStyleColor(ImGuiCol_FrameBgActive, frameBgAct);
        ImGui::PushStyleColor(ImGuiCol_Border, border);
        ImGui::PushStyleColor(ImGuiCol_Text, text);
        bool changed = ImGui::InputText(label, buffer, buf_size);
        ImGui::PopStyleColor(5);
        return changed;
    }

    bool UI::DragFloat(const char* label, float* value, float speed, float min, float max) {
        ImVec4 grab       = GetTheme().getAccentColor();
        ImVec4 grabHov    = ImVec4(grab.x * 1.2f, grab.y * 1.2f, grab.z * 1.2f, grab.w);
        ImVec4 frameBg    = GetTheme().getColor(ImGuiCol_FrameBg);
        ImVec4 frameBgHov = GetTheme().getColor(ImGuiCol_FrameBgHovered);
        ImVec4 border     = GetTheme().getBorderColor();

        ImGui::PushStyleColor(ImGuiCol_SliderGrab, grab);
        ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, grabHov);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, frameBg);
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, frameBgHov);
        ImGui::PushStyleColor(ImGuiCol_Border, border);
        bool changed = ImGui::DragFloat(label, value, speed, min, max, "%.2f", ImGuiSliderFlags_None);
        ImGui::PopStyleColor(5);
        return changed;
    }

    bool UI::DragFloat3(const char* label, float* value, float speed) {
        ImVec4 grab       = GetTheme().getAccentColor();
        ImVec4 grabHov    = ImVec4(grab.x * 1.2f, grab.y * 1.2f, grab.z * 1.2f, grab.w);
        ImVec4 frameBg    = GetTheme().getColor(ImGuiCol_FrameBg);
        ImVec4 frameBgHov = GetTheme().getColor(ImGuiCol_FrameBgHovered);
        ImVec4 border     = GetTheme().getBorderColor();

        ImGui::PushStyleColor(ImGuiCol_SliderGrab, grab);
        ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, grabHov);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, frameBg);
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, frameBgHov);
        ImGui::PushStyleColor(ImGuiCol_Border, border);
        bool changed = ImGui::DragFloat3(label, value, speed, -1e10f, 1e10f, "%.2f");
        ImGui::PopStyleColor(5);
        return changed;
    }

    bool UI::DragFloat3(const char* label, float* value, float speed, float min, float max) {
        ImVec4 grab       = GetTheme().getAccentColor();
        ImVec4 grabHov    = ImVec4(grab.x * 1.2f, grab.y * 1.2f, grab.z * 1.2f, grab.w);
        ImVec4 frameBg    = GetTheme().getColor(ImGuiCol_FrameBg);
        ImVec4 frameBgHov = GetTheme().getColor(ImGuiCol_FrameBgHovered);
        ImVec4 border     = GetTheme().getBorderColor();

        ImGui::PushStyleColor(ImGuiCol_SliderGrab, grab);
        ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, grabHov);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, frameBg);
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, frameBgHov);
        ImGui::PushStyleColor(ImGuiCol_Border, border);
        bool changed = ImGui::DragFloat3(label, value, speed, min, max, "%.2f");
        ImGui::PopStyleColor(5);
        return changed;
    }

    bool UI::ColorEdit3(const char* label, float* col) {
        ImVec4 frameBg    = GetTheme().getColor(ImGuiCol_FrameBg);
        ImVec4 frameBgHov = GetTheme().getColor(ImGuiCol_FrameBgHovered);
        ImVec4 border     = GetTheme().getBorderColor();

        ImGui::PushStyleColor(ImGuiCol_FrameBg, frameBg);
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, frameBgHov);
        ImGui::PushStyleColor(ImGuiCol_Border, border);
        bool changed = ImGui::ColorEdit3(label, col, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel);
        ImGui::PopStyleColor(3);
        return changed;
    }

    bool UI::InputFloat(const char* label, float* value, float step, float stepFast, const char* format) {
        ImVec4 frameBg    = GetTheme().getColor(ImGuiCol_FrameBg);
        ImVec4 frameBgHov = GetTheme().getColor(ImGuiCol_FrameBgHovered);
        ImVec4 frameBgAct = GetTheme().getColor(ImGuiCol_FrameBgActive);
        ImVec4 border     = GetTheme().getBorderColor();
        ImVec4 text       = GetTheme().getTextColor();

        ImGui::PushStyleColor(ImGuiCol_FrameBg, frameBg);
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, frameBgHov);
        ImGui::PushStyleColor(ImGuiCol_FrameBgActive, frameBgAct);
        ImGui::PushStyleColor(ImGuiCol_Border, border);
        ImGui::PushStyleColor(ImGuiCol_Text, text);
        bool changed = ImGui::InputFloat(label, value, step, stepFast, format);
        ImGui::PopStyleColor(5);
        return changed;
    }

    bool UI::SliderInt(const char* label, int* value, int min, int max) {
        ImVec4 grab       = GetTheme().getAccentColor();
        ImVec4 grabHov    = ImVec4(grab.x * 1.2f, grab.y * 1.2f, grab.z * 1.2f, grab.w);
        ImVec4 frameBg    = GetTheme().getColor(ImGuiCol_FrameBg);
        ImVec4 frameBgHov = GetTheme().getColor(ImGuiCol_FrameBgHovered);
        ImVec4 border     = GetTheme().getBorderColor();

        ImGui::PushStyleColor(ImGuiCol_SliderGrab, grab);
        ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, grabHov);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, frameBg);
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, frameBgHov);
        ImGui::PushStyleColor(ImGuiCol_Border, border);
        bool changed = ImGui::SliderInt(label, value, min, max);
        ImGui::PopStyleColor(5);
        return changed;
    }

    bool UI::InputInt(const char* label, int* value, int step, int stepFast) {
        ImVec4 frameBg    = GetTheme().getColor(ImGuiCol_FrameBg);
        ImVec4 frameBgHov = GetTheme().getColor(ImGuiCol_FrameBgHovered);
        ImVec4 frameBgAct = GetTheme().getColor(ImGuiCol_FrameBgActive);
        ImVec4 border     = GetTheme().getBorderColor();
        ImVec4 text       = GetTheme().getTextColor();

        ImGui::PushStyleColor(ImGuiCol_FrameBg, frameBg);
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, frameBgHov);
        ImGui::PushStyleColor(ImGuiCol_FrameBgActive, frameBgAct);
        ImGui::PushStyleColor(ImGuiCol_Border, border);
        ImGui::PushStyleColor(ImGuiCol_Text, text);
        bool changed = ImGui::InputInt(label, value, step, stepFast);
        ImGui::PopStyleColor(5);
        return changed;
    }

    bool UI::Combo(const char* label, int* current_index, const char* const items[], int count) {
        ImVec4 frameBg    = GetTheme().getColor(ImGuiCol_FrameBg);
        ImVec4 frameBgHov = GetTheme().getColor(ImGuiCol_FrameBgHovered);
        ImVec4 border     = GetTheme().getBorderColor();
        ImVec4 text       = GetTheme().getTextColor();

        ImGui::PushStyleColor(ImGuiCol_FrameBg, frameBg);
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, frameBgHov);
        ImGui::PushStyleColor(ImGuiCol_Border, border);
        ImGui::PushStyleColor(ImGuiCol_Text, text);
        bool changed = ImGui::Combo(label, current_index, items, count);
        ImGui::PopStyleColor(4);
        return changed;
    }

    bool UI::Combo(const char* label, int* current_index,
        std::function<int(const char* const*& out_items)> get_items_callback) {
        ImVec4 frameBg    = GetTheme().getColor(ImGuiCol_FrameBg);
        ImVec4 frameBgHov = GetTheme().getColor(ImGuiCol_FrameBgHovered);
        ImVec4 border     = GetTheme().getBorderColor();
        ImVec4 text       = GetTheme().getTextColor();

        ImGui::PushStyleColor(ImGuiCol_FrameBg, frameBg);
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, frameBgHov);
        ImGui::PushStyleColor(ImGuiCol_Border, border);
        ImGui::PushStyleColor(ImGuiCol_Text, text);
        const char* const* items   = nullptr;
        int                count   = get_items_callback(items);
        bool               changed = ImGui::Combo(label, current_index, items, count);
        ImGui::PopStyleColor(4);
        return changed;
    }

    bool UI::Checkbox(const char* label, bool* value) {
        ImVec4 checkMark  = GetTheme().getAccentColor();
        ImVec4 frameBg    = GetTheme().getColor(ImGuiCol_FrameBg);
        ImVec4 frameBgHov = GetTheme().getColor(ImGuiCol_FrameBgHovered);
        ImVec4 border     = GetTheme().getBorderColor();
        ImVec4 text       = GetTheme().getTextColor();

        ImGui::PushStyleColor(ImGuiCol_CheckMark, checkMark);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, frameBg);
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, frameBgHov);
        ImGui::PushStyleColor(ImGuiCol_Border, border);
        ImGui::PushStyleColor(ImGuiCol_Text, text);
        bool changed = ImGui::Checkbox(label, value);
        ImGui::PopStyleColor(5);
        return changed;
    }

    // ======================================================================
    // Display Helpers
    // ======================================================================

    void UI::TextDisabled(const char* text) {
        ImVec4 disabled = GetTheme().getDisabledColor();
        ImGui::TextColored(disabled, "%s", text);
    }

    void UI::TextDisabledWithTooltip(const char* text, const char* tooltip) {
        TextDisabled(text);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", tooltip);
        }
    }

    void UI::TextAccent(const char* text) {
        ImVec4 accent = GetTheme().getAccentColor();
        ImGui::TextColored(accent, "%s", text);
    }

    void UI::TextColored(const char* text, ImVec4 color) {
        ImGui::TextColored(color, "%s", text);
    }

    void UI::ProgressBar(float fraction, ImVec2 size) {
        ImVec4 grab    = GetTheme().getAccentColor();
        ImVec4 frameBg = GetTheme().getColor(ImGuiCol_FrameBg);
        ImVec4 border  = GetTheme().getBorderColor();

        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, grab);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, frameBg);
        ImGui::PushStyleColor(ImGuiCol_Border, border);
        ImGui::ProgressBar(fraction, size);
        ImGui::PopStyleColor(3);
    }

    void UI::InfoTooltip(const char* tooltip) {
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            ImGui::Text("%s", tooltip);
            ImGui::EndTooltip();
        }
    }

    void UI::WarningTooltip(const char* tooltip) {
        ImVec4 warning = ImVec4(1.0f, 0.8f, 0.2f, 1.0f);
        ImGui::TextColored(warning, "(!)");
        if (ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            ImGui::Text("%s", tooltip);
            ImGui::EndTooltip();
        }
    }

    // ======================================================================
    // Layout Helpers
    // ======================================================================

    void UI::PushThemeStyle() {
        if (!s_activeWorkspace)
            return;
        s_activeWorkspace->getThemeSystem().pushStyle();
    }

    void UI::PopThemeStyle() {
        if (!s_activeWorkspace)
            return;
        s_activeWorkspace->getThemeSystem().popStyle();
    }

    ImVec4 UI::GetFrameTimeColor(float frameTimeMs) {
        return GetTheme().getFrameTimeColor(frameTimeMs);
    }

    ImVec4 UI::GetAccentColor() {
        return GetTheme().getAccentColor();
    }

    ImVec4 UI::GetDisabledColor() {
        return GetTheme().getDisabledColor();
    }

    ImVec4 UI::GetBorderColor() {
        return GetTheme().getBorderColor();
    }

    ImVec4 UI::GetSeparatorColor() {
        return GetTheme().getSeparatorColor();
    }

    ImVec4 UI::GetTextColor() {
        return GetTheme().getTextColor();
    }

    // ======================================================================
    // Popup / Menu Helpers
    // ======================================================================

    bool UI::PopupButton(const char* label, const char* popup_id) {
        if (SmallButton(label)) {
            ImGui::OpenPopup(popup_id);
            return true;
        }
        return false;
    }

    bool UI::AddButton(const char* label) {
        ImGui::TextColored(GetAccentColor(), "+");
        ImGui::SameLine(0.0f, 2.0f);
        return SmallButton(label);
    }

    bool UI::DeleteButton(const char* label) {
        ImVec4 warning = ImVec4(1.0f, 0.4f, 0.4f, 1.0f);
        ImGui::TextColored(warning, "x");
        ImGui::SameLine(0.0f, 2.0f);
        return SmallButton(label);
    }

    bool UI::ResetButton(const char* label) {
        ImVec4 accent = GetAccentColor();
        ImGui::TextColored(accent, "\u21BA");
        ImGui::SameLine(0.0f, 2.0f);
        return SmallButton(label);
    }

    // Helper to apply section header styling
    void UI::ApplySectionStyle(bool open) {
        // This is called internally by Section() if needed
        (void) open;
    }

}  // namespace engine::ui
