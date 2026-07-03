#include "Editor/ui/Workspace/ThemeSystem.hpp"

#include <algorithm>
#include <cmath>
#include <sstream>

#include "Engine/Core/Logger.hpp"

#include "Editor/ui/Workspace/ThemeLoader.hpp"

namespace engine {

    ImGuiStyle ThemeSystem::currentStyle_;

    ImVec4 ThemeSystem::parseHexColor(const std::string& hex) const {
        ImVec4 result = {0.0f, 0.0f, 0.0f, 1.0f};
        if (hex.size() == 7) {
            result.x = std::stoi(hex.substr(1, 2), nullptr, 16) / 255.0f;
            result.y = std::stoi(hex.substr(3, 2), nullptr, 16) / 255.0f;
            result.z = std::stoi(hex.substr(5, 2), nullptr, 16) / 255.0f;
        } else if (hex.size() == 9) {
            result.x = std::stoi(hex.substr(1, 2), nullptr, 16) / 255.0f;
            result.y = std::stoi(hex.substr(3, 2), nullptr, 16) / 255.0f;
            result.z = std::stoi(hex.substr(5, 2), nullptr, 16) / 255.0f;
            result.w = std::stoi(hex.substr(7, 2), nullptr, 16) / 255.0f;
        }
        return result;
    }

    Theme ThemeSystem::parseTheme(const nlohmann::json& j) const {
        Theme theme;
        theme.name         = j.value("name", "Unknown");
        theme.accent_color = j.value("accent_color", "#66b3ff");

        auto s                      = j.value("style", nlohmann::json::object());
        theme.style.window_rounding = s.value("window_rounding", 4.0f);
        theme.style.frame_rounding  = s.value("frame_rounding", 2.0f);
        theme.style.grab_rounding   = s.value("grab_rounding", 1.2f);
        theme.style.tab_rounding    = s.value("tab_rounding", 2.0f);
        theme.style.frame_padding_x = s.value("frame_padding_x", 8.0f);
        theme.style.frame_padding_y = s.value("frame_padding_y", 4.0f);
        theme.style.item_spacing_x  = s.value("item_spacing_x", 8.0f);
        theme.style.item_spacing_y  = s.value("item_spacing_y", 8.0f);
        theme.style.window_padding  = s.value("window_padding", 8.0f);
        theme.style.grab_min_size   = s.value("grab_min_size", 12.0f);

        return theme;
    }

    void ThemeSystem::autoApplyAccentColor(const ImVec4& accent_color) {
        auto clamp = [](float val) { return std::clamp(val, 0.0f, 1.0f); };

        ImVec4 hovered = {
            clamp(accent_color.x * 0.85f),
            clamp(accent_color.y * 0.85f),
            clamp(accent_color.z * 0.85f),
            accent_color.w};
        ImVec4 active = {
            clamp(accent_color.x * 0.70f),
            clamp(accent_color.y * 0.70f),
            clamp(accent_color.z * 0.70f),
            accent_color.w};
        ImVec4 muted = {
            clamp(accent_color.x * 0.15f),
            clamp(accent_color.y * 0.15f),
            clamp(accent_color.z * 0.15f),
            0.45f};
        ImVec4 scrollGrab = {
            clamp(accent_color.x * 0.5f),
            clamp(accent_color.y * 0.5f),
            clamp(accent_color.z * 0.5f),
            0.70f};
        ImVec4 scrollGrabHovered = {
            clamp(accent_color.x * 0.7f),
            clamp(accent_color.y * 0.7f),
            clamp(accent_color.z * 0.7f),
            0.85f};
        ImVec4 scrollGrabActive = {
            clamp(accent_color.x),
            clamp(accent_color.y),
            clamp(accent_color.z),
            1.00f};

        currentStyle_.Colors[ImGuiCol_Header]        = accent_color;
        currentStyle_.Colors[ImGuiCol_HeaderHovered] = hovered;
        currentStyle_.Colors[ImGuiCol_HeaderActive]  = active;

        currentStyle_.Colors[ImGuiCol_Button]        = accent_color;
        currentStyle_.Colors[ImGuiCol_ButtonHovered] = hovered;
        currentStyle_.Colors[ImGuiCol_ButtonActive]  = active;

        currentStyle_.Colors[ImGuiCol_FrameBg]        = muted;
        currentStyle_.Colors[ImGuiCol_FrameBgHovered] = hovered;
        currentStyle_.Colors[ImGuiCol_FrameBgActive]  = active;

        currentStyle_.Colors[ImGuiCol_ScrollbarGrab]        = scrollGrab;
        currentStyle_.Colors[ImGuiCol_ScrollbarGrabHovered] = scrollGrabHovered;
        currentStyle_.Colors[ImGuiCol_ScrollbarGrabActive]  = scrollGrabActive;

        currentStyle_.Colors[ImGuiCol_SliderGrab]       = active;
        currentStyle_.Colors[ImGuiCol_SliderGrabActive] = accent_color;
        currentStyle_.Colors[ImGuiCol_CheckMark]        = accent_color;

        currentStyle_.Colors[ImGuiCol_Tab]                 = hovered;
        currentStyle_.Colors[ImGuiCol_TabHovered]          = accent_color;
        currentStyle_.Colors[ImGuiCol_TabActive]           = active;
        currentStyle_.Colors[ImGuiCol_TabUnfocused]        = ImVec4(hovered.x, hovered.y, hovered.z, 0.4f);
        currentStyle_.Colors[ImGuiCol_TabUnfocusedActive]  = ImVec4(active.x, active.y, active.z, 0.6f);
        currentStyle_.Colors[ImGuiCol_TabSelectedOverline] = accent_color;

        currentStyle_.Colors[ImGuiCol_TitleBgActive]     = active;
        currentStyle_.Colors[ImGuiCol_SeparatorHovered]  = hovered;
        currentStyle_.Colors[ImGuiCol_SeparatorActive]   = accent_color;
        currentStyle_.Colors[ImGuiCol_ResizeGrip]        = hovered;
        currentStyle_.Colors[ImGuiCol_ResizeGripHovered] = accent_color;
        currentStyle_.Colors[ImGuiCol_ResizeGripActive]  = active;

        currentStyle_.Colors[ImGuiCol_DockingPreview] = ImVec4(accent_color.x, accent_color.y, accent_color.z, 0.35f);

        currentStyle_.Colors[ImGuiCol_NavHighlight]   = accent_color;
        currentStyle_.Colors[ImGuiCol_TextSelectedBg] = ImVec4(accent_color.x, accent_color.y, accent_color.z, 0.35f);

        currentStyle_.Colors[ImGuiCol_PlotLines]        = accent_color;
        currentStyle_.Colors[ImGuiCol_PlotLinesHovered] = hovered;
    }

    void ThemeSystem::applyBaseColors(const nlohmann::json& colors) {
        auto get = [&colors](const std::string& key) -> std::string {
            return colors.value(key, std::string(""));
        };

        auto col = [this](ImGuiCol idx, const std::string& hex) {
            if (!hex.empty()) {
                currentStyle_.Colors[idx] = parseHexColor(hex);
            }
        };

        col(ImGuiCol_WindowBg, get("window_bg"));
        col(ImGuiCol_TitleBg, get("title_bg"));
        col(ImGuiCol_MenuBarBg, get("menu_bar_bg"));
        col(ImGuiCol_Border, get("border"));
        col(ImGuiCol_BorderShadow, get("border_shadow"));
        col(ImGuiCol_Text, get("text"));
        col(ImGuiCol_TextDisabled, get("text_disabled"));
        col(ImGuiCol_ScrollbarBg, get("scrollbar_bg"));
        col(ImGuiCol_ScrollbarGrab, get("scrollbar_grab"));
        col(ImGuiCol_ScrollbarGrabHovered, get("scrollbar_grab_hovered"));
        col(ImGuiCol_ScrollbarGrabActive, get("scrollbar_grab_active"));
        col(ImGuiCol_Tab, get("tab"));
        col(ImGuiCol_TabActive, get("tab_active"));
        col(ImGuiCol_TabUnfocused, get("tab_unfocused"));
        col(ImGuiCol_TabUnfocusedActive, get("tab_unfocused_active"));
        col(ImGuiCol_DragDropTarget, get("drag_drop_target"));
        col(ImGuiCol_Header, get("header"));
        col(ImGuiCol_Separator, get("separator"));
        col(ImGuiCol_SeparatorActive, get("separator_active"));
        col(ImGuiCol_ResizeGrip, get("resize_grip"));
        col(ImGuiCol_ResizeGripActive, get("resize_grip_active"));
        col(ImGuiCol_TextSelectedBg, get("text_selected_bg"));
        col(ImGuiCol_PlotLines, get("plot_lines"));
        col(ImGuiCol_ModalWindowDimBg, get("modal_window_dim_bg"));
    }

    ThemeSystem::ThemeSystem() {
        applyTheme("Midnight");
    }

    void ThemeSystem::applyTheme(const std::string& theme_name) {
        try {
            auto  json  = ThemeLoader::loadByName("assets/editor/themes", theme_name);
            Theme theme = parseTheme(json);
            applyTheme(theme);
        } catch (const std::exception& e) {
            engine::Logger::error(engine::LogChannel::General, "ThemeSystem: Failed to load theme '", theme_name, "': ", e.what());
        }
    }

    void ThemeSystem::applyTheme(const Theme& theme) {
        currentThemeName_   = theme.name;
        currentAccentColor_ = parseHexColor(theme.accent_color);

        auto json = ThemeLoader::loadByName("assets/editor/themes", theme.name);
        applyBaseColors(json.value("base_colors", nlohmann::json::object()));

        autoApplyAccentColor(currentAccentColor_);

        auto& s                        = theme.style;
        currentStyle_.WindowRounding   = s.window_rounding;
        currentStyle_.FrameRounding    = s.frame_rounding;
        currentStyle_.GrabRounding     = s.grab_rounding;
        currentStyle_.TabRounding      = s.tab_rounding;
        currentStyle_.FramePadding     = ImVec2(s.frame_padding_x, s.frame_padding_y);
        currentStyle_.ItemSpacing      = ImVec2(s.item_spacing_x, s.item_spacing_y);
        currentStyle_.ItemInnerSpacing = ImVec2(s.item_spacing_x, s.item_spacing_y);
        currentStyle_.WindowPadding    = ImVec2(s.window_padding, s.window_padding);
        currentStyle_.GrabMinSize      = s.grab_min_size;

        ImGui::GetStyle() = currentStyle_;
        engine::Logger::info(engine::LogChannel::General, "ThemeSystem: Applied theme '", theme.name, "'");
    }

    ImVec4 ThemeSystem::getAccentColor() const {
        return currentAccentColor_;
    }

    void ThemeSystem::pushStyle() const {
        ImGuiStyle& style = currentStyle_;
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, style.WindowRounding);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, style.WindowBorderSize);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, style.WindowPadding);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, style.FrameRounding);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, style.FramePadding);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, style.ItemSpacing);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, style.ItemInnerSpacing);
        ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing, style.IndentSpacing);
        ImGui::PushStyleVar(ImGuiStyleVar_GrabMinSize, style.GrabMinSize);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowMinSize, ImVec2(0.0f, 0.0f));
        ImGui::PushStyleColor(ImGuiCol_WindowBg, currentStyle_.Colors[ImGuiCol_WindowBg]);
        ImGui::PushStyleColor(ImGuiCol_TitleBg, currentStyle_.Colors[ImGuiCol_TitleBg]);
        ImGui::PushStyleColor(ImGuiCol_TitleBgActive, currentStyle_.Colors[ImGuiCol_TitleBgActive]);
        ImGui::PushStyleColor(ImGuiCol_MenuBarBg, currentStyle_.Colors[ImGuiCol_MenuBarBg]);
        ImGui::PushStyleColor(ImGuiCol_Border, currentStyle_.Colors[ImGuiCol_Border]);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, currentStyle_.Colors[ImGuiCol_FrameBg]);
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, currentStyle_.Colors[ImGuiCol_FrameBgHovered]);
        ImGui::PushStyleColor(ImGuiCol_FrameBgActive, currentStyle_.Colors[ImGuiCol_FrameBgActive]);
        ImGui::PushStyleColor(ImGuiCol_Button, currentStyle_.Colors[ImGuiCol_Button]);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, currentStyle_.Colors[ImGuiCol_ButtonHovered]);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, currentStyle_.Colors[ImGuiCol_ButtonActive]);
        ImGui::PushStyleColor(ImGuiCol_Text, currentStyle_.Colors[ImGuiCol_Text]);
        ImGui::PushStyleColor(ImGuiCol_TextDisabled, currentStyle_.Colors[ImGuiCol_TextDisabled]);
        ImGui::PushStyleColor(ImGuiCol_CheckMark, currentStyle_.Colors[ImGuiCol_CheckMark]);
        ImGui::PushStyleColor(ImGuiCol_ScrollbarBg, currentStyle_.Colors[ImGuiCol_ScrollbarBg]);
        ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab, currentStyle_.Colors[ImGuiCol_ScrollbarGrab]);
        ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabHovered, currentStyle_.Colors[ImGuiCol_ScrollbarGrabHovered]);
        ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabActive, currentStyle_.Colors[ImGuiCol_ScrollbarGrabActive]);
        ImGui::PushStyleColor(ImGuiCol_Tab, currentStyle_.Colors[ImGuiCol_Tab]);
        ImGui::PushStyleColor(ImGuiCol_TabHovered, currentStyle_.Colors[ImGuiCol_TabHovered]);
        ImGui::PushStyleColor(ImGuiCol_TabActive, currentStyle_.Colors[ImGuiCol_TabActive]);
        ImGui::PushStyleColor(ImGuiCol_Header, currentStyle_.Colors[ImGuiCol_Header]);
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, currentStyle_.Colors[ImGuiCol_HeaderHovered]);
        ImGui::PushStyleColor(ImGuiCol_HeaderActive, currentStyle_.Colors[ImGuiCol_HeaderActive]);
        ImGui::PushStyleColor(ImGuiCol_Separator, currentStyle_.Colors[ImGuiCol_Separator]);
        ImGui::PushStyleColor(ImGuiCol_SeparatorHovered, currentStyle_.Colors[ImGuiCol_SeparatorHovered]);
        ImGui::PushStyleColor(ImGuiCol_SeparatorActive, currentStyle_.Colors[ImGuiCol_SeparatorActive]);
        ImGui::PushStyleColor(ImGuiCol_ResizeGrip, currentStyle_.Colors[ImGuiCol_ResizeGrip]);
        ImGui::PushStyleColor(ImGuiCol_ResizeGripHovered, currentStyle_.Colors[ImGuiCol_ResizeGripHovered]);
        ImGui::PushStyleColor(ImGuiCol_ResizeGripActive, currentStyle_.Colors[ImGuiCol_ResizeGripActive]);
        ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, currentStyle_.Colors[ImGuiCol_TextSelectedBg]);
        ImGui::PushStyleColor(ImGuiCol_DockingPreview, currentStyle_.Colors[ImGuiCol_DockingPreview]);
        ImGui::PushStyleColor(ImGuiCol_DragDropTarget, currentStyle_.Colors[ImGuiCol_DragDropTarget]);
        ImGui::PushStyleColor(ImGuiCol_NavHighlight, currentStyle_.Colors[ImGuiCol_NavHighlight]);
        ImGui::PushStyleColor(ImGuiCol_PlotLines, currentStyle_.Colors[ImGuiCol_PlotLines]);
        ImGui::PushStyleColor(ImGuiCol_PlotLinesHovered, currentStyle_.Colors[ImGuiCol_PlotLinesHovered]);
        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, currentStyle_.Colors[ImGuiCol_PlotHistogram]);
        ImGui::PushStyleColor(ImGuiCol_PlotHistogramHovered, currentStyle_.Colors[ImGuiCol_PlotHistogramHovered]);
        ImGui::PushStyleColor(ImGuiCol_ModalWindowDimBg, currentStyle_.Colors[ImGuiCol_ModalWindowDimBg]);
    }

    void ThemeSystem::popStyle() const {
        ImGui::PopStyleColor(39);
        ImGui::PopStyleVar(10);
    }

    ImVec4 ThemeSystem::getFrameTimeColor(float frameTimeMs) const {
        if (frameTimeMs < 16.67f) {
            return ImVec4(0.3f, 1.0f, 0.3f, 1.0f);
        } else if (frameTimeMs < 33.33f) {
            return ImVec4(1.0f, 0.85f, 0.0f, 1.0f);
        } else {
            return ImVec4(1.0f, 0.3f, 0.3f, 1.0f);
        }
    }

    ImVec4 ThemeSystem::getTextColor() const {
        return currentStyle_.Colors[ImGuiCol_Text];
    }

    ImVec4 ThemeSystem::getDisabledColor() const {
        return currentStyle_.Colors[ImGuiCol_TextDisabled];
    }

    ImVec4 ThemeSystem::getBorderColor() const {
        return currentStyle_.Colors[ImGuiCol_Border];
    }

    ImVec4 ThemeSystem::getSeparatorColor() const {
        return currentStyle_.Colors[ImGuiCol_Separator];
    }

    ImVec4 ThemeSystem::getColor(ImGuiCol idx) const {
        ImU32 col = ImGui::GetColorU32(idx);
        return ImVec4(
            ((col >> 0) & 0xFF) / 255.0f,
            ((col >> 8) & 0xFF) / 255.0f,
            ((col >> 16) & 0xFF) / 255.0f,
            ((col >> 24) & 0xFF) / 255.0f);
    }

}  // namespace engine
