#ifndef EDITOR_WORKSPACE_THEME_SYSTEM_HPP
#define EDITOR_WORKSPACE_THEME_SYSTEM_HPP

#include <imgui.h>

#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace engine {

    struct ThemeStyle {
        float window_rounding = 4.0f;
        float frame_rounding  = 2.0f;
        float grab_rounding   = 1.2f;
        float tab_rounding    = 2.0f;
        float frame_padding_x = 8.0f;
        float frame_padding_y = 4.0f;
        float item_spacing_x  = 8.0f;
        float item_spacing_y  = 8.0f;
        float window_padding  = 8.0f;
        float grab_min_size   = 12.0f;
    };

    struct Theme {
        std::string name;
        std::string accent_color;
        ThemeStyle  style;
    };

    class ThemeSystem {
       public:
        ThemeSystem();
        ~ThemeSystem() = default;

        /**
     * @brief Apply a theme by name.
     * @param theme_name Theme name (e.g. "dark", "light", "midnight").
     */
        void applyTheme(const std::string& theme_name);

        /**
     * @brief Apply a theme from a pre-loaded Theme struct.
     */
        void applyTheme(const Theme& theme);

        /**
     * @brief Get the current theme name.
     */
        std::string getThemeName() const {
            return currentThemeName_;
        }

        /**
     * @brief Get the current accent color.
     */
        ImVec4 getAccentColor() const;

        /**
     * @brief Get the current ImGui style reference.
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
     */
        ImVec4 getColor(ImGuiCol idx) const;

       private:
        void autoApplyAccentColor(const ImVec4& accent_color);

        ImVec4 parseHexColor(const std::string& hex) const;

        Theme parseTheme(const nlohmann::json& j) const;

        void applyBaseColors(const nlohmann::json& colors);

        std::string currentThemeName_ = "dark";
        ImVec4      currentAccentColor_;

        static ImGuiStyle currentStyle_;
    };

}  // namespace engine

#endif
