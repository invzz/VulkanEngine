#ifndef EDITOR_WORKSPACE_THEME_SYSTEM_HPP
#define EDITOR_WORKSPACE_THEME_SYSTEM_HPP

#include <imgui.h>

namespace engine {

    /**
 * @brief Centralized theme/styling system for the editor.
 *
 * Manages ImGui style colors, rounding, spacing, and typography.
 * Replaces the scattered style presets in ToolbarPanel with a
 * single source of truth.
 */
    class ThemeSystem {
       public:
        ThemeSystem();
        ~ThemeSystem() = default;

        /**
     * @brief Apply a theme preset.
     * @param preset 0=Dark, 1=Light, 2=Midnight
     */
        void applyPreset(int preset);

        /**
     * @brief Get the current theme preset.
     */
        int getPreset() const {
            return currentPreset_;
        }

        /**
     * @brief Get the current ImGui style reference.
     * Use this to access/modify current style values.
     */
        ImGuiStyle& getStyle() {
            return currentStyle_;
        }

        /**
     * @brief Push the current theme style vars onto the ImGui stack.
     * Call at the start of panel rendering.
     */
        void pushStyle() const;

        /**
     * @brief Pop the theme style vars from the ImGui stack.
     * Call at the end of panel rendering.
     */
        void popStyle() const;

        /**
     * @brief Get the current frame time color (green/yellow/red).
     */
        ImVec4 getFrameTimeColor(float frameTimeMs) const;

        /**
     * @brief Get the accent color for the current theme.
     */
        ImVec4 getAccentColor() const;

        /**
     * @brief Get the text color for the current theme.
     */
        ImVec4 getTextColor() const;

        /**
     * @brief Get the disabled text color for the current theme.
     */
        ImVec4 getDisabledColor() const;

        /**
     * @brief Get the border color for the current theme.
     */
        ImVec4 getBorderColor() const;

        /**
     * @brief Get the separator color for the current theme.
     */
        ImVec4 getSeparatorColor() const;

        /**
     * @brief Get a specific ImGui color from the current theme.
     * @param idx ImGui color index.
     * @return The color value.
     */
        ImVec4 getColor(ImGuiCol idx) const;

       private:
        int currentPreset_ = 0;

        // Dark theme colors
        static const ImVec4 DARK_WINDOW_BG;
        static const ImVec4 DARK_TITLE_BG;
        static const ImVec4 DARK_TITLE_BG_ACTIVE;
        static const ImVec4 DARK_FRAME_BG;
        static const ImVec4 DARK_FRAME_BG_HOVERED;
        static const ImVec4 DARK_FRAME_BG_ACTIVE;
        static const ImVec4 DARK_BUTTON;
        static const ImVec4 DARK_BUTTON_HOVERED;
        static const ImVec4 DARK_BUTTON_ACTIVE;
        static const ImVec4 DARK_TEXT;
        static const ImVec4 DARK_TEXT_DISABLED;
        static const ImVec4 DARK_CHECK_MARK;
        static const ImVec4 DARK_BORDER;
        static const ImVec4 DARK_SEPARATOR;
        static const ImVec4 DARK_HEADER;
        static const ImVec4 DARK_HEADER_HOVERED;
        static const ImVec4 DARK_HEADER_ACTIVE;
        static const ImVec4 DARK_TAB;
        static const ImVec4 DARK_TAB_ACTIVE;
        static const ImVec4 DARK_TAB_HOVERED;
        static const ImVec4 DARK_SCROLLBAR;
        static const ImVec4 DARK_SCROLLBAR_HOVERED;
        static const ImVec4 DARK_SCROLLBAR_ACTIVE;
        static const ImVec4 DARK_RESIZE_GRIP;
        static const ImVec4 DARK_RESIZE_GRIP_HOVERED;
        static const ImVec4 DARK_RESIZE_GRIP_ACTIVE;
        static const ImVec4 DARK_TEXT_SELECTED_BG;
        static const ImVec4 DARK_DOCKING_PREVIEW;
        static const ImVec4 DARK_DRAG_DROP_TARGET;
        static const ImVec4 DARK_NAV_HIGHLIGHT;
        static const ImVec4 DARK_PLOT_LINES;
        static const ImVec4 DARK_PLOT_LINES_HOVERED;
        static const ImVec4 DARK_PLOT_HISTOGRAM;
        static const ImVec4 DARK_PLOT_HISTOGRAM_HOVERED;
        static const ImVec4 DARK_MODAL_WINDOW_DIM_BG;
        static const float  DARK_WINDOW_ROUNDING;
        static const float  DARK_FRAME_ROUNDING;
        static const float  DARK_GRAB_ROUNDING;
        static const float  DARK_TAB_ROUNDING;

        // Light theme colors
        static const ImVec4 LIGHT_WINDOW_BG;
        static const ImVec4 LIGHT_TITLE_BG;
        static const ImVec4 LIGHT_TITLE_BG_ACTIVE;
        static const ImVec4 LIGHT_FRAME_BG;
        static const ImVec4 LIGHT_FRAME_BG_HOVERED;
        static const ImVec4 LIGHT_FRAME_BG_ACTIVE;
        static const ImVec4 LIGHT_BUTTON;
        static const ImVec4 LIGHT_BUTTON_HOVERED;
        static const ImVec4 LIGHT_BUTTON_ACTIVE;
        static const ImVec4 LIGHT_TEXT;
        static const ImVec4 LIGHT_TEXT_DISABLED;
        static const ImVec4 LIGHT_CHECK_MARK;
        static const ImVec4 LIGHT_BORDER;
        static const ImVec4 LIGHT_SEPARATOR;
        static const ImVec4 LIGHT_HEADER;
        static const ImVec4 LIGHT_HEADER_HOVERED;
        static const ImVec4 LIGHT_HEADER_ACTIVE;
        static const ImVec4 LIGHT_TAB;
        static const ImVec4 LIGHT_TAB_ACTIVE;
        static const ImVec4 LIGHT_TAB_HOVERED;
        static const ImVec4 LIGHT_SCROLLBAR;
        static const ImVec4 LIGHT_SCROLLBAR_HOVERED;
        static const ImVec4 LIGHT_SCROLLBAR_ACTIVE;
        static const ImVec4 LIGHT_RESIZE_GRIP;
        static const ImVec4 LIGHT_RESIZE_GRIP_HOVERED;
        static const ImVec4 LIGHT_RESIZE_GRIP_ACTIVE;
        static const ImVec4 LIGHT_TEXT_SELECTED_BG;
        static const ImVec4 LIGHT_DOCKING_PREVIEW;
        static const ImVec4 LIGHT_DRAG_DROP_TARGET;
        static const ImVec4 LIGHT_NAV_HIGHLIGHT;
        static const ImVec4 LIGHT_PLOT_LINES;
        static const ImVec4 LIGHT_PLOT_LINES_HOVERED;
        static const ImVec4 LIGHT_PLOT_HISTOGRAM;
        static const ImVec4 LIGHT_PLOT_HISTOGRAM_HOVERED;
        static const ImVec4 LIGHT_MODAL_WINDOW_DIM_BG;
        static const float  LIGHT_WINDOW_ROUNDING;
        static const float  LIGHT_FRAME_ROUNDING;
        static const float  LIGHT_GRAB_ROUNDING;
        static const float  LIGHT_TAB_ROUNDING;

        // Midnight theme colors
        static const ImVec4 MIDNIGHT_WINDOW_BG;
        static const ImVec4 MIDNIGHT_TITLE_BG;
        static const ImVec4 MIDNIGHT_TITLE_BG_ACTIVE;
        static const ImVec4 MIDNIGHT_FRAME_BG;
        static const ImVec4 MIDNIGHT_FRAME_BG_HOVERED;
        static const ImVec4 MIDNIGHT_FRAME_BG_ACTIVE;
        static const ImVec4 MIDNIGHT_BUTTON;
        static const ImVec4 MIDNIGHT_BUTTON_HOVERED;
        static const ImVec4 MIDNIGHT_BUTTON_ACTIVE;
        static const ImVec4 MIDNIGHT_TEXT;
        static const ImVec4 MIDNIGHT_TEXT_DISABLED;
        static const ImVec4 MIDNIGHT_CHECK_MARK;
        static const ImVec4 MIDNIGHT_BORDER;
        static const ImVec4 MIDNIGHT_SEPARATOR;
        static const ImVec4 MIDNIGHT_HEADER;
        static const ImVec4 MIDNIGHT_HEADER_HOVERED;
        static const ImVec4 MIDNIGHT_HEADER_ACTIVE;
        static const ImVec4 MIDNIGHT_TAB;
        static const ImVec4 MIDNIGHT_TAB_ACTIVE;
        static const ImVec4 MIDNIGHT_TAB_HOVERED;
        static const ImVec4 MIDNIGHT_SCROLLBAR;
        static const ImVec4 MIDNIGHT_SCROLLBAR_HOVERED;
        static const ImVec4 MIDNIGHT_SCROLLBAR_ACTIVE;
        static const ImVec4 MIDNIGHT_RESIZE_GRIP;
        static const ImVec4 MIDNIGHT_RESIZE_GRIP_HOVERED;
        static const ImVec4 MIDNIGHT_RESIZE_GRIP_ACTIVE;
        static const ImVec4 MIDNIGHT_TEXT_SELECTED_BG;
        static const ImVec4 MIDNIGHT_DOCKING_PREVIEW;
        static const ImVec4 MIDNIGHT_DRAG_DROP_TARGET;
        static const ImVec4 MIDNIGHT_NAV_HIGHLIGHT;
        static const ImVec4 MIDNIGHT_PLOT_LINES;
        static const ImVec4 MIDNIGHT_PLOT_LINES_HOVERED;
        static const ImVec4 MIDNIGHT_PLOT_HISTOGRAM;
        static const ImVec4 MIDNIGHT_PLOT_HISTOGRAM_HOVERED;
        static const ImVec4 MIDNIGHT_MODAL_WINDOW_DIM_BG;
        static const float  MIDNIGHT_WINDOW_ROUNDING;
        static const float  MIDNIGHT_FRAME_ROUNDING;
        static const float  MIDNIGHT_GRAB_ROUNDING;
        static const float  MIDNIGHT_TAB_ROUNDING;

        // Current computed style (mirrors ImGui::GetStyle())
        static ImGuiStyle currentStyle_;
    };

}  // namespace engine

#endif  // EDITOR_WORKSPACE_THEME_SYSTEM_HPP
