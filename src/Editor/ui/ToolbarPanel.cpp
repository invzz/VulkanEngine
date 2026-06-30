#include "Editor/ui/ToolbarPanel.hpp"

#include <algorithm>
#include <cmath>
#include <imgui.h>

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
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoScrollWithMouse |
            ImGuiWindowFlags_NoNav |
            ImGuiWindowFlags_NoFocusOnAppearing |
            ImGuiWindowFlags_NoBringToFrontOnFocus |
            ImGuiWindowFlags_NoBackground;

        // Render toolbar as a transparent overlay
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4.0f, 2.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(3.0f, 2.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, ImVec2(2.0f, 2.0f));

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

        // --- Style preset selector ---
        const char* styleNames[] = {"Dark", "Light", "Midnight"};
        if (ImGui::Combo("##style", &stylePreset_, styleNames, IM_ARRAYSIZE(styleNames))) {
            applyStylePreset(stylePreset_);
        }

        // --- Viewport mode toggle ---
        bool nav = (frameInfo.viewportMode == ViewportMode::Navigation);
        ImGui::SameLine(0.0f, 16.0f);
        if (ImGui::Checkbox("Navigation", &nav)) {
            frameInfo.viewportMode = nav ? ViewportMode::Navigation : ViewportMode::Picking;
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

        ImGui::PopStyleVar(5);

        ImGui::End();
    }

    void ToolbarPanel::applyStylePreset(int preset) {
        ImGuiStyle& style = ImGui::GetStyle();
        float       round = 4.0f;

        switch (preset) {
            case 0: {
                // Dark theme (default-ish)
                style.Colors[ImGuiCol_WindowBg]             = ImVec4(0.12f, 0.12f, 0.14f, 0.95f);
                style.Colors[ImGuiCol_WindowBg]             = ImVec4(0.10f, 0.10f, 0.12f, 1.0f);
                style.Colors[ImGuiCol_TitleBg]              = ImVec4(0.08f, 0.08f, 0.10f, 1.0f);
                style.Colors[ImGuiCol_TitleBgActive]        = ImVec4(0.15f, 0.15f, 0.18f, 1.0f);
                style.Colors[ImGuiCol_TitleBgCollapsed]     = ImVec4(0.10f, 0.10f, 0.12f, 1.0f);
                style.Colors[ImGuiCol_MenuBarBg]            = ImVec4(0.14f, 0.14f, 0.16f, 1.0f);
                style.Colors[ImGuiCol_Border]               = ImVec4(0.30f, 0.30f, 0.35f, 0.75f);
                style.Colors[ImGuiCol_BorderShadow]         = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
                style.Colors[ImGuiCol_FrameBg]              = ImVec4(0.20f, 0.20f, 0.23f, 1.0f);
                style.Colors[ImGuiCol_FrameBgHovered]       = ImVec4(0.30f, 0.30f, 0.35f, 1.0f);
                style.Colors[ImGuiCol_FrameBgActive]        = ImVec4(0.35f, 0.35f, 0.40f, 1.0f);
                style.Colors[ImGuiCol_Button]               = ImVec4(0.25f, 0.25f, 0.28f, 1.0f);
                style.Colors[ImGuiCol_ButtonHovered]        = ImVec4(0.35f, 0.35f, 0.40f, 1.0f);
                style.Colors[ImGuiCol_ButtonActive]         = ImVec4(0.45f, 0.45f, 0.50f, 1.0f);
                style.Colors[ImGuiCol_Text]                 = ImVec4(0.90f, 0.90f, 0.90f, 1.0f);
                style.Colors[ImGuiCol_TextDisabled]         = ImVec4(0.50f, 0.50f, 0.55f, 1.0f);
                style.Colors[ImGuiCol_CheckMark]            = ImVec4(0.40f, 0.70f, 1.0f, 1.0f);
                style.Colors[ImGuiCol_ScrollbarBg]          = ImVec4(0.08f, 0.08f, 0.10f, 1.0f);
                style.Colors[ImGuiCol_ScrollbarGrab]        = ImVec4(0.30f, 0.30f, 0.35f, 1.0f);
                style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.40f, 0.40f, 0.45f, 1.0f);
                style.Colors[ImGuiCol_ScrollbarGrabActive]  = ImVec4(0.50f, 0.50f, 0.55f, 1.0f);
                style.Colors[ImGuiCol_Tab]                  = ImVec4(0.15f, 0.15f, 0.18f, 1.0f);
                style.Colors[ImGuiCol_TabHovered]           = ImVec4(0.30f, 0.30f, 0.35f, 1.0f);
                style.Colors[ImGuiCol_TabActive]            = ImVec4(0.20f, 0.20f, 0.24f, 1.0f);
                style.Colors[ImGuiCol_TabUnfocused]         = ImVec4(0.12f, 0.12f, 0.14f, 1.0f);
                style.Colors[ImGuiCol_TabUnfocusedActive]   = ImVec4(0.15f, 0.15f, 0.18f, 1.0f);
                style.Colors[ImGuiCol_DockingPreview]       = ImVec4(0.40f, 0.70f, 1.0f, 0.7f);
                style.Colors[ImGuiCol_DragDropTarget]       = ImVec4(1.0f, 0.85f, 0.0f, 1.0f);
                style.Colors[ImGuiCol_NavHighlight]         = ImVec4(0.40f, 0.70f, 1.0f, 1.0f);
                style.Colors[ImGuiCol_Header]               = ImVec4(0.25f, 0.25f, 0.28f, 1.0f);
                style.Colors[ImGuiCol_HeaderHovered]        = ImVec4(0.35f, 0.35f, 0.40f, 1.0f);
                style.Colors[ImGuiCol_HeaderActive]         = ImVec4(0.40f, 0.40f, 0.45f, 1.0f);
                style.Colors[ImGuiCol_Separator]            = ImVec4(0.25f, 0.25f, 0.30f, 1.0f);
                style.Colors[ImGuiCol_SeparatorHovered]     = ImVec4(0.35f, 0.35f, 0.40f, 1.0f);
                style.Colors[ImGuiCol_SeparatorActive]      = ImVec4(0.40f, 0.40f, 0.45f, 1.0f);
                style.Colors[ImGuiCol_ResizeGrip]           = ImVec4(0.30f, 0.30f, 0.35f, 0.5f);
                style.Colors[ImGuiCol_ResizeGripHovered]    = ImVec4(0.40f, 0.40f, 0.45f, 0.75f);
                style.Colors[ImGuiCol_ResizeGripActive]     = ImVec4(0.50f, 0.50f, 0.55f, 0.75f);
                style.Colors[ImGuiCol_PlotLines]            = ImVec4(0.60f, 0.60f, 0.60f, 1.0f);
                style.Colors[ImGuiCol_PlotLinesHovered]     = ImVec4(0.80f, 0.80f, 0.80f, 1.0f);
                style.Colors[ImGuiCol_PlotHistogram]        = ImVec4(0.40f, 0.60f, 0.80f, 1.0f);
                style.Colors[ImGuiCol_PlotHistogramHovered] = ImVec4(0.60f, 0.80f, 1.0f, 1.0f);
                style.Colors[ImGuiCol_TextSelectedBg]       = ImVec4(0.30f, 0.40f, 0.60f, 0.5f);
                style.Colors[ImGuiCol_ModalWindowDimBg]     = ImVec4(0.00f, 0.00f, 0.00f, 0.5f);
                style.WindowRounding                        = round;
                style.FrameRounding                         = round * 0.5f;
                style.GrabRounding                          = round * 0.3f;
                style.TabRounding                           = round * 0.5f;
                break;
            }
            case 1: {
                // Light theme
                style.Colors[ImGuiCol_WindowBg]             = ImVec4(0.95f, 0.95f, 0.96f, 1.0f);
                style.Colors[ImGuiCol_WindowBg]             = ImVec4(0.90f, 0.90f, 0.92f, 1.0f);
                style.Colors[ImGuiCol_TitleBg]              = ImVec4(0.80f, 0.80f, 0.83f, 1.0f);
                style.Colors[ImGuiCol_TitleBgActive]        = ImVec4(0.70f, 0.70f, 0.73f, 1.0f);
                style.Colors[ImGuiCol_TitleBgCollapsed]     = ImVec4(0.85f, 0.85f, 0.87f, 1.0f);
                style.Colors[ImGuiCol_MenuBarBg]            = ImVec4(0.88f, 0.88f, 0.90f, 1.0f);
                style.Colors[ImGuiCol_Border]               = ImVec4(0.60f, 0.60f, 0.63f, 0.75f);
                style.Colors[ImGuiCol_BorderShadow]         = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
                style.Colors[ImGuiCol_FrameBg]              = ImVec4(0.85f, 0.85f, 0.87f, 1.0f);
                style.Colors[ImGuiCol_FrameBgHovered]       = ImVec4(0.75f, 0.75f, 0.78f, 1.0f);
                style.Colors[ImGuiCol_FrameBgActive]        = ImVec4(0.65f, 0.65f, 0.68f, 1.0f);
                style.Colors[ImGuiCol_Button]               = ImVec4(0.70f, 0.70f, 0.73f, 1.0f);
                style.Colors[ImGuiCol_ButtonHovered]        = ImVec4(0.60f, 0.60f, 0.63f, 1.0f);
                style.Colors[ImGuiCol_ButtonActive]         = ImVec4(0.50f, 0.50f, 0.53f, 1.0f);
                style.Colors[ImGuiCol_Text]                 = ImVec4(0.15f, 0.15f, 0.18f, 1.0f);
                style.Colors[ImGuiCol_TextDisabled]         = ImVec4(0.45f, 0.45f, 0.48f, 1.0f);
                style.Colors[ImGuiCol_CheckMark]            = ImVec4(0.20f, 0.50f, 0.90f, 1.0f);
                style.Colors[ImGuiCol_ScrollbarBg]          = ImVec4(0.90f, 0.90f, 0.92f, 1.0f);
                style.Colors[ImGuiCol_ScrollbarGrab]        = ImVec4(0.60f, 0.60f, 0.63f, 1.0f);
                style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.50f, 0.50f, 0.53f, 1.0f);
                style.Colors[ImGuiCol_ScrollbarGrabActive]  = ImVec4(0.40f, 0.40f, 0.43f, 1.0f);
                style.Colors[ImGuiCol_Tab]                  = ImVec4(0.82f, 0.82f, 0.84f, 1.0f);
                style.Colors[ImGuiCol_TabHovered]           = ImVec4(0.70f, 0.70f, 0.73f, 1.0f);
                style.Colors[ImGuiCol_TabActive]            = ImVec4(0.75f, 0.75f, 0.78f, 1.0f);
                style.Colors[ImGuiCol_TabUnfocused]         = ImVec4(0.88f, 0.88f, 0.90f, 1.0f);
                style.Colors[ImGuiCol_TabUnfocusedActive]   = ImVec4(0.82f, 0.82f, 0.84f, 1.0f);
                style.Colors[ImGuiCol_DockingPreview]       = ImVec4(0.20f, 0.50f, 0.90f, 0.7f);
                style.Colors[ImGuiCol_DragDropTarget]       = ImVec4(0.80f, 0.60f, 0.0f, 1.0f);
                style.Colors[ImGuiCol_NavHighlight]         = ImVec4(0.20f, 0.50f, 0.90f, 1.0f);
                style.Colors[ImGuiCol_Header]               = ImVec4(0.70f, 0.70f, 0.73f, 1.0f);
                style.Colors[ImGuiCol_HeaderHovered]        = ImVec4(0.60f, 0.60f, 0.63f, 1.0f);
                style.Colors[ImGuiCol_HeaderActive]         = ImVec4(0.50f, 0.50f, 0.53f, 1.0f);
                style.Colors[ImGuiCol_Separator]            = ImVec4(0.70f, 0.70f, 0.73f, 1.0f);
                style.Colors[ImGuiCol_SeparatorHovered]     = ImVec4(0.60f, 0.60f, 0.63f, 1.0f);
                style.Colors[ImGuiCol_SeparatorActive]      = ImVec4(0.50f, 0.50f, 0.53f, 1.0f);
                style.Colors[ImGuiCol_ResizeGrip]           = ImVec4(0.60f, 0.60f, 0.63f, 0.5f);
                style.Colors[ImGuiCol_ResizeGripHovered]    = ImVec4(0.50f, 0.50f, 0.53f, 0.75f);
                style.Colors[ImGuiCol_ResizeGripActive]     = ImVec4(0.40f, 0.40f, 0.43f, 0.75f);
                style.Colors[ImGuiCol_PlotLines]            = ImVec4(0.40f, 0.40f, 0.40f, 1.0f);
                style.Colors[ImGuiCol_PlotLinesHovered]     = ImVec4(0.60f, 0.60f, 0.60f, 1.0f);
                style.Colors[ImGuiCol_PlotHistogram]        = ImVec4(0.30f, 0.50f, 0.70f, 1.0f);
                style.Colors[ImGuiCol_PlotHistogramHovered] = ImVec4(0.40f, 0.60f, 0.80f, 1.0f);
                style.Colors[ImGuiCol_TextSelectedBg]       = ImVec4(0.40f, 0.50f, 0.70f, 0.3f);
                style.Colors[ImGuiCol_ModalWindowDimBg]     = ImVec4(0.00f, 0.00f, 0.00f, 0.3f);
                style.WindowRounding                        = round;
                style.FrameRounding                         = round * 0.5f;
                style.GrabRounding                          = round * 0.3f;
                style.TabRounding                           = round * 0.5f;
                break;
            }
            case 2: {
                // Midnight theme (deep blue-purple)
                style.Colors[ImGuiCol_WindowBg]             = ImVec4(0.06f, 0.06f, 0.10f, 0.95f);
                style.Colors[ImGuiCol_WindowBg]             = ImVec4(0.05f, 0.05f, 0.08f, 1.0f);
                style.Colors[ImGuiCol_TitleBg]              = ImVec4(0.04f, 0.04f, 0.07f, 1.0f);
                style.Colors[ImGuiCol_TitleBgActive]        = ImVec4(0.10f, 0.10f, 0.16f, 1.0f);
                style.Colors[ImGuiCol_TitleBgCollapsed]     = ImVec4(0.06f, 0.06f, 0.10f, 1.0f);
                style.Colors[ImGuiCol_MenuBarBg]            = ImVec4(0.08f, 0.08f, 0.12f, 1.0f);
                style.Colors[ImGuiCol_Border]               = ImVec4(0.15f, 0.15f, 0.22f, 0.75f);
                style.Colors[ImGuiCol_BorderShadow]         = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
                style.Colors[ImGuiCol_FrameBg]              = ImVec4(0.08f, 0.08f, 0.12f, 1.0f);
                style.Colors[ImGuiCol_FrameBgHovered]       = ImVec4(0.12f, 0.12f, 0.18f, 1.0f);
                style.Colors[ImGuiCol_FrameBgActive]        = ImVec4(0.16f, 0.16f, 0.22f, 1.0f);
                style.Colors[ImGuiCol_Button]               = ImVec4(0.10f, 0.10f, 0.15f, 1.0f);
                style.Colors[ImGuiCol_ButtonHovered]        = ImVec4(0.15f, 0.15f, 0.22f, 1.0f);
                style.Colors[ImGuiCol_ButtonActive]         = ImVec4(0.20f, 0.20f, 0.28f, 1.0f);
                style.Colors[ImGuiCol_Text]                 = ImVec4(0.80f, 0.82f, 0.90f, 1.0f);
                style.Colors[ImGuiCol_TextDisabled]         = ImVec4(0.40f, 0.42f, 0.50f, 1.0f);
                style.Colors[ImGuiCol_CheckMark]            = ImVec4(0.50f, 0.65f, 1.0f, 1.0f);
                style.Colors[ImGuiCol_ScrollbarBg]          = ImVec4(0.04f, 0.04f, 0.06f, 1.0f);
                style.Colors[ImGuiCol_ScrollbarGrab]        = ImVec4(0.15f, 0.15f, 0.22f, 1.0f);
                style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.20f, 0.20f, 0.28f, 1.0f);
                style.Colors[ImGuiCol_ScrollbarGrabActive]  = ImVec4(0.25f, 0.25f, 0.35f, 1.0f);
                style.Colors[ImGuiCol_Tab]                  = ImVec4(0.07f, 0.07f, 0.11f, 1.0f);
                style.Colors[ImGuiCol_TabHovered]           = ImVec4(0.15f, 0.15f, 0.22f, 1.0f);
                style.Colors[ImGuiCol_TabActive]            = ImVec4(0.10f, 0.10f, 0.16f, 1.0f);
                style.Colors[ImGuiCol_TabUnfocused]         = ImVec4(0.06f, 0.06f, 0.10f, 1.0f);
                style.Colors[ImGuiCol_TabUnfocusedActive]   = ImVec4(0.08f, 0.08f, 0.12f, 1.0f);
                style.Colors[ImGuiCol_DockingPreview]       = ImVec4(0.50f, 0.65f, 1.0f, 0.7f);
                style.Colors[ImGuiCol_DragDropTarget]       = ImVec4(1.0f, 0.80f, 0.10f, 1.0f);
                style.Colors[ImGuiCol_NavHighlight]         = ImVec4(0.50f, 0.65f, 1.0f, 1.0f);
                style.Colors[ImGuiCol_Header]               = ImVec4(0.12f, 0.12f, 0.18f, 1.0f);
                style.Colors[ImGuiCol_HeaderHovered]        = ImVec4(0.18f, 0.18f, 0.25f, 1.0f);
                style.Colors[ImGuiCol_HeaderActive]         = ImVec4(0.22f, 0.22f, 0.30f, 1.0f);
                style.Colors[ImGuiCol_Separator]            = ImVec4(0.12f, 0.12f, 0.18f, 1.0f);
                style.Colors[ImGuiCol_SeparatorHovered]     = ImVec4(0.18f, 0.18f, 0.25f, 1.0f);
                style.Colors[ImGuiCol_SeparatorActive]      = ImVec4(0.22f, 0.22f, 0.30f, 1.0f);
                style.Colors[ImGuiCol_ResizeGrip]           = ImVec4(0.15f, 0.15f, 0.22f, 0.5f);
                style.Colors[ImGuiCol_ResizeGripHovered]    = ImVec4(0.20f, 0.20f, 0.28f, 0.75f);
                style.Colors[ImGuiCol_ResizeGripActive]     = ImVec4(0.25f, 0.25f, 0.35f, 0.75f);
                style.Colors[ImGuiCol_PlotLines]            = ImVec4(0.45f, 0.45f, 0.55f, 1.0f);
                style.Colors[ImGuiCol_PlotLinesHovered]     = ImVec4(0.60f, 0.60f, 0.75f, 1.0f);
                style.Colors[ImGuiCol_PlotHistogram]        = ImVec4(0.25f, 0.40f, 0.65f, 1.0f);
                style.Colors[ImGuiCol_PlotHistogramHovered] = ImVec4(0.35f, 0.55f, 0.80f, 1.0f);
                style.Colors[ImGuiCol_TextSelectedBg]       = ImVec4(0.20f, 0.30f, 0.50f, 0.5f);
                style.Colors[ImGuiCol_ModalWindowDimBg]     = ImVec4(0.00f, 0.00f, 0.00f, 0.6f);
                style.WindowRounding                        = round;
                style.FrameRounding                         = round * 0.5f;
                style.GrabRounding                          = round * 0.3f;
                style.TabRounding                           = round * 0.5f;
                break;
            }
            default:
                break;
        }

        // Force style rebuild
        // ImGui::StyleColorsDark();
        // applyStylePreset(preset);
    }

}  // namespace engine
