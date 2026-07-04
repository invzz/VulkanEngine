#pragma once
#include <string>
#include "IconsFontAwesome6.h"
#include <imgui.h>
class IconFont
{
public:
    static inline ImFont* FontAwesome = nullptr;
    static void Initialize(float fontSize = 16.0f, const char* fontPath = "fa-solid-900.ttf")
    {
        ImGuiIO& io = ImGui::GetIO();
        ImFontConfig config;
        config.MergeMode = true;
        config.GlyphMinAdvanceX = fontSize;
        config.PixelSnapH = true;
        static const ImWchar iconRanges[] = { ICON_MIN_FA, ICON_MAX_FA, 0 };
        FontAwesome = io.Fonts->AddFontFromFileTTF(fontPath, fontSize, &config, iconRanges);
        if (!FontAwesome)
            IM_ASSERT(0 && "Failed to load Font Awesome!");
    }
    
    static void Text(const char* icon)
    {
        ImGui::Text("%s", icon);
    }
    static void Text(const char* icon, const char* text)
    {
        ImGui::Text("%s %s", icon, text);
    }
    
    static bool Button(const char* icon, const char* label = "")
    {
        if (label && label[0] != '\0')
            return ImGui::Button((std::string(icon) + " " + label).c_str());
        return ImGui::Button(icon);
    }
    static bool Button(const char* icon, const char* label, const char* id)
    {
        return ImGui::Button((std::string(icon) + " " + label + "###" + id).c_str());
    }
    
    static bool MenuItem(const char* icon, const char* label, bool selected = false, bool enabled = true)
    {
        return ImGui::MenuItem((std::string(icon) + " " + label).c_str(), nullptr, selected, enabled);
    }
    
    static void PushFont()
    {
        if (FontAwesome)
            ImGui::PushFont(FontAwesome);
    }
    static void PopFont()
    {
        ImGui::PopFont();
    }
};
