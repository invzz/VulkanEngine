#include "Editor/Workspace/ThemeSystem.hpp"

#include <algorithm>
#include <cmath>

namespace engine {

// Static color constants - Dark theme
const ImVec4 ThemeSystem::DARK_WINDOW_BG{0.10f, 0.10f, 0.12f, 1.0f};
const ImVec4 ThemeSystem::DARK_TITLE_BG{0.08f, 0.08f, 0.10f, 1.0f};
const ImVec4 ThemeSystem::DARK_TITLE_BG_ACTIVE{0.15f, 0.15f, 0.18f, 1.0f};
const ImVec4 ThemeSystem::DARK_FRAME_BG{0.20f, 0.20f, 0.23f, 1.0f};
const ImVec4 ThemeSystem::DARK_FRAME_BG_HOVERED{0.30f, 0.30f, 0.35f, 1.0f};
const ImVec4 ThemeSystem::DARK_FRAME_BG_ACTIVE{0.35f, 0.35f, 0.40f, 1.0f};
const ImVec4 ThemeSystem::DARK_BUTTON{0.25f, 0.25f, 0.28f, 1.0f};
const ImVec4 ThemeSystem::DARK_BUTTON_HOVERED{0.35f, 0.35f, 0.40f, 1.0f};
const ImVec4 ThemeSystem::DARK_BUTTON_ACTIVE{0.45f, 0.45f, 0.50f, 1.0f};
const ImVec4 ThemeSystem::DARK_TEXT{0.90f, 0.90f, 0.90f, 1.0f};
const ImVec4 ThemeSystem::DARK_TEXT_DISABLED{0.50f, 0.50f, 0.55f, 1.0f};
const ImVec4 ThemeSystem::DARK_CHECK_MARK{0.40f, 0.70f, 1.0f, 1.0f};
const ImVec4 ThemeSystem::DARK_BORDER{0.30f, 0.30f, 0.35f, 0.75f};
const ImVec4 ThemeSystem::DARK_SEPARATOR{0.25f, 0.25f, 0.30f, 1.0f};
const ImVec4 ThemeSystem::DARK_HEADER{0.25f, 0.25f, 0.28f, 1.0f};
const ImVec4 ThemeSystem::DARK_HEADER_HOVERED{0.35f, 0.35f, 0.40f, 1.0f};
const ImVec4 ThemeSystem::DARK_HEADER_ACTIVE{0.40f, 0.40f, 0.45f, 1.0f};
const ImVec4 ThemeSystem::DARK_TAB{0.15f, 0.15f, 0.18f, 1.0f};
const ImVec4 ThemeSystem::DARK_TAB_ACTIVE{0.20f, 0.20f, 0.24f, 1.0f};
const ImVec4 ThemeSystem::DARK_TAB_HOVERED{0.30f, 0.30f, 0.35f, 1.0f};
const ImVec4 ThemeSystem::DARK_SCROLLBAR{0.30f, 0.30f, 0.35f, 1.0f};
const ImVec4 ThemeSystem::DARK_SCROLLBAR_HOVERED{0.40f, 0.40f, 0.45f, 1.0f};
const ImVec4 ThemeSystem::DARK_SCROLLBAR_ACTIVE{0.50f, 0.50f, 0.55f, 1.0f};
const ImVec4 ThemeSystem::DARK_RESIZE_GRIP{0.30f, 0.30f, 0.35f, 0.5f};
const ImVec4 ThemeSystem::DARK_RESIZE_GRIP_HOVERED{0.40f, 0.40f, 0.45f, 0.75f};
const ImVec4 ThemeSystem::DARK_RESIZE_GRIP_ACTIVE{0.50f, 0.50f, 0.55f, 0.75f};
const ImVec4 ThemeSystem::DARK_TEXT_SELECTED_BG{0.30f, 0.40f, 0.60f, 0.5f};
const ImVec4 ThemeSystem::DARK_DOCKING_PREVIEW{0.40f, 0.70f, 1.0f, 0.7f};
const ImVec4 ThemeSystem::DARK_DRAG_DROP_TARGET{1.0f, 0.85f, 0.0f, 1.0f};
const ImVec4 ThemeSystem::DARK_NAV_HIGHLIGHT{0.40f, 0.70f, 1.0f, 1.0f};
const ImVec4 ThemeSystem::DARK_PLOT_LINES{0.60f, 0.60f, 0.60f, 1.0f};
const ImVec4 ThemeSystem::DARK_PLOT_LINES_HOVERED{0.80f, 0.80f, 0.80f, 1.0f};
const ImVec4 ThemeSystem::DARK_PLOT_HISTOGRAM{0.40f, 0.60f, 0.80f, 1.0f};
const ImVec4 ThemeSystem::DARK_PLOT_HISTOGRAM_HOVERED{0.60f, 0.80f, 1.0f, 1.0f};
const ImVec4 ThemeSystem::DARK_MODAL_WINDOW_DIM_BG{0.00f, 0.00f, 0.00f, 0.5f};
const float  ThemeSystem::DARK_WINDOW_ROUNDING = 4.0f;
const float  ThemeSystem::DARK_FRAME_ROUNDING  = 2.0f;
const float  ThemeSystem::DARK_GRAB_ROUNDING   = 1.2f;
const float  ThemeSystem::DARK_TAB_ROUNDING    = 2.0f;

// Static color constants - Light theme
const ImVec4 ThemeSystem::LIGHT_WINDOW_BG{0.90f, 0.90f, 0.92f, 1.0f};
const ImVec4 ThemeSystem::LIGHT_TITLE_BG{0.80f, 0.80f, 0.83f, 1.0f};
const ImVec4 ThemeSystem::LIGHT_TITLE_BG_ACTIVE{0.70f, 0.70f, 0.73f, 1.0f};
const ImVec4 ThemeSystem::LIGHT_FRAME_BG{0.85f, 0.85f, 0.87f, 1.0f};
const ImVec4 ThemeSystem::LIGHT_FRAME_BG_HOVERED{0.75f, 0.75f, 0.78f, 1.0f};
const ImVec4 ThemeSystem::LIGHT_FRAME_BG_ACTIVE{0.65f, 0.65f, 0.68f, 1.0f};
const ImVec4 ThemeSystem::LIGHT_BUTTON{0.70f, 0.70f, 0.73f, 1.0f};
const ImVec4 ThemeSystem::LIGHT_BUTTON_HOVERED{0.60f, 0.60f, 0.63f, 1.0f};
const ImVec4 ThemeSystem::LIGHT_BUTTON_ACTIVE{0.50f, 0.50f, 0.53f, 1.0f};
const ImVec4 ThemeSystem::LIGHT_TEXT{0.15f, 0.15f, 0.18f, 1.0f};
const ImVec4 ThemeSystem::LIGHT_TEXT_DISABLED{0.45f, 0.45f, 0.48f, 1.0f};
const ImVec4 ThemeSystem::LIGHT_CHECK_MARK{0.20f, 0.50f, 0.90f, 1.0f};
const ImVec4 ThemeSystem::LIGHT_BORDER{0.60f, 0.60f, 0.63f, 0.75f};
const ImVec4 ThemeSystem::LIGHT_SEPARATOR{0.70f, 0.70f, 0.73f, 1.0f};
const ImVec4 ThemeSystem::LIGHT_HEADER{0.70f, 0.70f, 0.73f, 1.0f};
const ImVec4 ThemeSystem::LIGHT_HEADER_HOVERED{0.60f, 0.60f, 0.63f, 1.0f};
const ImVec4 ThemeSystem::LIGHT_HEADER_ACTIVE{0.50f, 0.50f, 0.53f, 1.0f};
const ImVec4 ThemeSystem::LIGHT_TAB{0.82f, 0.82f, 0.84f, 1.0f};
const ImVec4 ThemeSystem::LIGHT_TAB_ACTIVE{0.75f, 0.75f, 0.78f, 1.0f};
const ImVec4 ThemeSystem::LIGHT_TAB_HOVERED{0.70f, 0.70f, 0.73f, 1.0f};
const ImVec4 ThemeSystem::LIGHT_SCROLLBAR{0.60f, 0.60f, 0.63f, 1.0f};
const ImVec4 ThemeSystem::LIGHT_SCROLLBAR_HOVERED{0.50f, 0.50f, 0.53f, 1.0f};
const ImVec4 ThemeSystem::LIGHT_SCROLLBAR_ACTIVE{0.40f, 0.40f, 0.43f, 1.0f};
const ImVec4 ThemeSystem::LIGHT_RESIZE_GRIP{0.60f, 0.60f, 0.63f, 0.5f};
const ImVec4 ThemeSystem::LIGHT_RESIZE_GRIP_HOVERED{0.50f, 0.50f, 0.53f, 0.75f};
const ImVec4 ThemeSystem::LIGHT_RESIZE_GRIP_ACTIVE{0.40f, 0.40f, 0.43f, 0.75f};
const ImVec4 ThemeSystem::LIGHT_TEXT_SELECTED_BG{0.40f, 0.50f, 0.70f, 0.3f};
const ImVec4 ThemeSystem::LIGHT_DOCKING_PREVIEW{0.20f, 0.50f, 0.90f, 0.7f};
const ImVec4 ThemeSystem::LIGHT_DRAG_DROP_TARGET{0.80f, 0.60f, 0.0f, 1.0f};
const ImVec4 ThemeSystem::LIGHT_NAV_HIGHLIGHT{0.20f, 0.50f, 0.90f, 1.0f};
const ImVec4 ThemeSystem::LIGHT_PLOT_LINES{0.40f, 0.40f, 0.40f, 1.0f};
const ImVec4 ThemeSystem::LIGHT_PLOT_LINES_HOVERED{0.60f, 0.60f, 0.60f, 1.0f};
const ImVec4 ThemeSystem::LIGHT_PLOT_HISTOGRAM{0.30f, 0.50f, 0.70f, 1.0f};
const ImVec4 ThemeSystem::LIGHT_PLOT_HISTOGRAM_HOVERED{0.40f, 0.60f, 0.80f, 1.0f};
const ImVec4 ThemeSystem::LIGHT_MODAL_WINDOW_DIM_BG{0.00f, 0.00f, 0.00f, 0.3f};
const float  ThemeSystem::LIGHT_WINDOW_ROUNDING = 4.0f;
const float  ThemeSystem::LIGHT_FRAME_ROUNDING  = 2.0f;
const float  ThemeSystem::LIGHT_GRAB_ROUNDING   = 1.2f;
const float  ThemeSystem::LIGHT_TAB_ROUNDING    = 2.0f;

// Static color constants - Midnight theme
const ImVec4 ThemeSystem::MIDNIGHT_WINDOW_BG{0.05f, 0.05f, 0.08f, 1.0f};
const ImVec4 ThemeSystem::MIDNIGHT_TITLE_BG{0.04f, 0.04f, 0.07f, 1.0f};
const ImVec4 ThemeSystem::MIDNIGHT_TITLE_BG_ACTIVE{0.10f, 0.10f, 0.16f, 1.0f};
const ImVec4 ThemeSystem::MIDNIGHT_FRAME_BG{0.08f, 0.08f, 0.12f, 1.0f};
const ImVec4 ThemeSystem::MIDNIGHT_FRAME_BG_HOVERED{0.12f, 0.12f, 0.18f, 1.0f};
const ImVec4 ThemeSystem::MIDNIGHT_FRAME_BG_ACTIVE{0.16f, 0.16f, 0.22f, 1.0f};
const ImVec4 ThemeSystem::MIDNIGHT_BUTTON{0.10f, 0.10f, 0.15f, 1.0f};
const ImVec4 ThemeSystem::MIDNIGHT_BUTTON_HOVERED{0.15f, 0.15f, 0.22f, 1.0f};
const ImVec4 ThemeSystem::MIDNIGHT_BUTTON_ACTIVE{0.20f, 0.20f, 0.28f, 1.0f};
const ImVec4 ThemeSystem::MIDNIGHT_TEXT{0.80f, 0.82f, 0.90f, 1.0f};
const ImVec4 ThemeSystem::MIDNIGHT_TEXT_DISABLED{0.40f, 0.42f, 0.50f, 1.0f};
const ImVec4 ThemeSystem::MIDNIGHT_CHECK_MARK{0.50f, 0.65f, 1.0f, 1.0f};
const ImVec4 ThemeSystem::MIDNIGHT_BORDER{0.15f, 0.15f, 0.22f, 0.75f};
const ImVec4 ThemeSystem::MIDNIGHT_SEPARATOR{0.12f, 0.12f, 0.18f, 1.0f};
const ImVec4 ThemeSystem::MIDNIGHT_HEADER{0.12f, 0.12f, 0.18f, 1.0f};
const ImVec4 ThemeSystem::MIDNIGHT_HEADER_HOVERED{0.18f, 0.18f, 0.25f, 1.0f};
const ImVec4 ThemeSystem::MIDNIGHT_HEADER_ACTIVE{0.22f, 0.22f, 0.30f, 1.0f};
const ImVec4 ThemeSystem::MIDNIGHT_TAB{0.07f, 0.07f, 0.11f, 1.0f};
const ImVec4 ThemeSystem::MIDNIGHT_TAB_ACTIVE{0.10f, 0.10f, 0.16f, 1.0f};
const ImVec4 ThemeSystem::MIDNIGHT_TAB_HOVERED{0.15f, 0.15f, 0.22f, 1.0f};
const ImVec4 ThemeSystem::MIDNIGHT_SCROLLBAR{0.15f, 0.15f, 0.22f, 1.0f};
const ImVec4 ThemeSystem::MIDNIGHT_SCROLLBAR_HOVERED{0.20f, 0.20f, 0.28f, 1.0f};
const ImVec4 ThemeSystem::MIDNIGHT_SCROLLBAR_ACTIVE{0.25f, 0.25f, 0.35f, 1.0f};
const ImVec4 ThemeSystem::MIDNIGHT_RESIZE_GRIP{0.15f, 0.15f, 0.22f, 0.5f};
const ImVec4 ThemeSystem::MIDNIGHT_RESIZE_GRIP_HOVERED{0.20f, 0.20f, 0.28f, 0.75f};
const ImVec4 ThemeSystem::MIDNIGHT_RESIZE_GRIP_ACTIVE{0.25f, 0.25f, 0.35f, 0.75f};
const ImVec4 ThemeSystem::MIDNIGHT_TEXT_SELECTED_BG{0.20f, 0.30f, 0.50f, 0.5f};
const ImVec4 ThemeSystem::MIDNIGHT_DOCKING_PREVIEW{0.50f, 0.65f, 1.0f, 0.7f};
const ImVec4 ThemeSystem::MIDNIGHT_DRAG_DROP_TARGET{1.0f, 0.80f, 0.10f, 1.0f};
const ImVec4 ThemeSystem::MIDNIGHT_NAV_HIGHLIGHT{0.50f, 0.65f, 1.0f, 1.0f};
const ImVec4 ThemeSystem::MIDNIGHT_PLOT_LINES{0.45f, 0.45f, 0.55f, 1.0f};
const ImVec4 ThemeSystem::MIDNIGHT_PLOT_LINES_HOVERED{0.60f, 0.60f, 0.75f, 1.0f};
const ImVec4 ThemeSystem::MIDNIGHT_PLOT_HISTOGRAM{0.25f, 0.40f, 0.65f, 1.0f};
const ImVec4 ThemeSystem::MIDNIGHT_PLOT_HISTOGRAM_HOVERED{0.35f, 0.55f, 0.80f, 1.0f};
const ImVec4 ThemeSystem::MIDNIGHT_MODAL_WINDOW_DIM_BG{0.00f, 0.00f, 0.00f, 0.6f};
const float  ThemeSystem::MIDNIGHT_WINDOW_ROUNDING = 4.0f;
const float  ThemeSystem::MIDNIGHT_FRAME_ROUNDING  = 2.0f;
const float  ThemeSystem::MIDNIGHT_GRAB_ROUNDING   = 1.2f;
const float  ThemeSystem::MIDNIGHT_TAB_ROUNDING    = 2.0f;

// Current computed style (mirrors ImGui::GetStyle())
ImGuiStyle ThemeSystem::currentStyle_;

ThemeSystem::ThemeSystem() {
    // Initialize with dark theme
    applyPreset(0);
}

void ThemeSystem::applyPreset(int preset) {
    currentPreset_ = preset;
    ImGuiStyle& style = currentStyle_;

    // Start with defaults
    style = ImGui::GetStyle();

    float round = 4.0f;

    switch (preset) {
        case 0: { // Dark
            style.Colors[ImGuiCol_WindowBg]             = DARK_WINDOW_BG;
            style.Colors[ImGuiCol_TitleBg]              = DARK_TITLE_BG;
            style.Colors[ImGuiCol_TitleBgActive]        = DARK_TITLE_BG_ACTIVE;
            style.Colors[ImGuiCol_MenuBarBg]            = ImVec4(0.14f, 0.14f, 0.16f, 1.0f);
            style.Colors[ImGuiCol_Border]               = DARK_BORDER;
            style.Colors[ImGuiCol_BorderShadow]         = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
            style.Colors[ImGuiCol_FrameBg]              = DARK_FRAME_BG;
            style.Colors[ImGuiCol_FrameBgHovered]       = DARK_FRAME_BG_HOVERED;
            style.Colors[ImGuiCol_FrameBgActive]        = DARK_FRAME_BG_ACTIVE;
            style.Colors[ImGuiCol_Button]               = DARK_BUTTON;
            style.Colors[ImGuiCol_ButtonHovered]        = DARK_BUTTON_HOVERED;
            style.Colors[ImGuiCol_ButtonActive]         = DARK_BUTTON_ACTIVE;
            style.Colors[ImGuiCol_Text]                 = DARK_TEXT;
            style.Colors[ImGuiCol_TextDisabled]         = DARK_TEXT_DISABLED;
            style.Colors[ImGuiCol_CheckMark]            = DARK_CHECK_MARK;
            style.Colors[ImGuiCol_ScrollbarBg]          = ImVec4(0.08f, 0.08f, 0.10f, 1.0f);
            style.Colors[ImGuiCol_ScrollbarGrab]        = DARK_SCROLLBAR;
            style.Colors[ImGuiCol_ScrollbarGrabHovered] = DARK_SCROLLBAR_HOVERED;
            style.Colors[ImGuiCol_ScrollbarGrabActive]  = DARK_SCROLLBAR_ACTIVE;
            style.Colors[ImGuiCol_Tab]                  = DARK_TAB;
            style.Colors[ImGuiCol_TabHovered]           = DARK_TAB_HOVERED;
            style.Colors[ImGuiCol_TabActive]            = DARK_TAB_ACTIVE;
            style.Colors[ImGuiCol_TabUnfocused]         = DARK_WINDOW_BG;
            style.Colors[ImGuiCol_TabUnfocusedActive]   = DARK_TAB_ACTIVE;
            style.Colors[ImGuiCol_DockingPreview]       = DARK_DOCKING_PREVIEW;
            style.Colors[ImGuiCol_DragDropTarget]       = DARK_DRAG_DROP_TARGET;
            style.Colors[ImGuiCol_NavHighlight]         = DARK_NAV_HIGHLIGHT;
            style.Colors[ImGuiCol_Header]               = DARK_HEADER;
            style.Colors[ImGuiCol_HeaderHovered]        = DARK_HEADER_HOVERED;
            style.Colors[ImGuiCol_HeaderActive]         = DARK_HEADER_ACTIVE;
            style.Colors[ImGuiCol_Separator]            = DARK_SEPARATOR;
            style.Colors[ImGuiCol_SeparatorHovered]     = DARK_FRAME_BG_HOVERED;
            style.Colors[ImGuiCol_SeparatorActive]      = DARK_FRAME_BG_ACTIVE;
            style.Colors[ImGuiCol_ResizeGrip]           = DARK_RESIZE_GRIP;
            style.Colors[ImGuiCol_ResizeGripHovered]    = DARK_RESIZE_GRIP_HOVERED;
            style.Colors[ImGuiCol_ResizeGripActive]     = DARK_RESIZE_GRIP_ACTIVE;
            style.Colors[ImGuiCol_PlotLines]            = DARK_PLOT_LINES;
            style.Colors[ImGuiCol_PlotLinesHovered]     = DARK_PLOT_LINES_HOVERED;
            style.Colors[ImGuiCol_PlotHistogram]        = DARK_PLOT_HISTOGRAM;
            style.Colors[ImGuiCol_PlotHistogramHovered] = DARK_PLOT_HISTOGRAM_HOVERED;
            style.Colors[ImGuiCol_TextSelectedBg]       = DARK_TEXT_SELECTED_BG;
            style.Colors[ImGuiCol_ModalWindowDimBg]     = DARK_MODAL_WINDOW_DIM_BG;
            style.WindowRounding = DARK_WINDOW_ROUNDING;
            style.FrameRounding  = DARK_FRAME_ROUNDING;
            style.GrabRounding   = DARK_GRAB_ROUNDING;
            style.TabRounding    = DARK_TAB_ROUNDING;
            break;
        }
        case 1: { // Light
            style.Colors[ImGuiCol_WindowBg]             = LIGHT_WINDOW_BG;
            style.Colors[ImGuiCol_TitleBg]              = LIGHT_TITLE_BG;
            style.Colors[ImGuiCol_TitleBgActive]        = LIGHT_TITLE_BG_ACTIVE;
            style.Colors[ImGuiCol_MenuBarBg]            = ImVec4(0.88f, 0.88f, 0.90f, 1.0f);
            style.Colors[ImGuiCol_Border]               = LIGHT_BORDER;
            style.Colors[ImGuiCol_BorderShadow]         = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
            style.Colors[ImGuiCol_FrameBg]              = LIGHT_FRAME_BG;
            style.Colors[ImGuiCol_FrameBgHovered]       = LIGHT_FRAME_BG_HOVERED;
            style.Colors[ImGuiCol_FrameBgActive]        = LIGHT_FRAME_BG_ACTIVE;
            style.Colors[ImGuiCol_Button]               = LIGHT_BUTTON;
            style.Colors[ImGuiCol_ButtonHovered]        = LIGHT_BUTTON_HOVERED;
            style.Colors[ImGuiCol_ButtonActive]         = LIGHT_BUTTON_ACTIVE;
            style.Colors[ImGuiCol_Text]                 = LIGHT_TEXT;
            style.Colors[ImGuiCol_TextDisabled]         = LIGHT_TEXT_DISABLED;
            style.Colors[ImGuiCol_CheckMark]            = LIGHT_CHECK_MARK;
            style.Colors[ImGuiCol_ScrollbarBg]          = LIGHT_WINDOW_BG;
            style.Colors[ImGuiCol_ScrollbarGrab]        = LIGHT_SCROLLBAR;
            style.Colors[ImGuiCol_ScrollbarGrabHovered] = LIGHT_SCROLLBAR_HOVERED;
            style.Colors[ImGuiCol_ScrollbarGrabActive]  = LIGHT_SCROLLBAR_ACTIVE;
            style.Colors[ImGuiCol_Tab]                  = LIGHT_TAB;
            style.Colors[ImGuiCol_TabHovered]           = LIGHT_TAB_HOVERED;
            style.Colors[ImGuiCol_TabActive]            = LIGHT_TAB_ACTIVE;
            style.Colors[ImGuiCol_TabUnfocused]         = ImVec4(0.88f, 0.88f, 0.90f, 1.0f);
            style.Colors[ImGuiCol_TabUnfocusedActive]   = LIGHT_TAB_ACTIVE;
            style.Colors[ImGuiCol_DockingPreview]       = LIGHT_DOCKING_PREVIEW;
            style.Colors[ImGuiCol_DragDropTarget]       = LIGHT_DRAG_DROP_TARGET;
            style.Colors[ImGuiCol_NavHighlight]         = LIGHT_NAV_HIGHLIGHT;
            style.Colors[ImGuiCol_Header]               = LIGHT_HEADER;
            style.Colors[ImGuiCol_HeaderHovered]        = LIGHT_HEADER_HOVERED;
            style.Colors[ImGuiCol_HeaderActive]         = LIGHT_HEADER_ACTIVE;
            style.Colors[ImGuiCol_Separator]            = LIGHT_SEPARATOR;
            style.Colors[ImGuiCol_SeparatorHovered]     = LIGHT_BORDER;
            style.Colors[ImGuiCol_SeparatorActive]      = ImVec4(0.50f, 0.50f, 0.53f, 1.0f);
            style.Colors[ImGuiCol_ResizeGrip]           = LIGHT_RESIZE_GRIP;
            style.Colors[ImGuiCol_ResizeGripHovered]    = LIGHT_RESIZE_GRIP_HOVERED;
            style.Colors[ImGuiCol_ResizeGripActive]     = LIGHT_RESIZE_GRIP_ACTIVE;
            style.Colors[ImGuiCol_PlotLines]            = LIGHT_PLOT_LINES;
            style.Colors[ImGuiCol_PlotLinesHovered]     = LIGHT_PLOT_LINES_HOVERED;
            style.Colors[ImGuiCol_PlotHistogram]        = LIGHT_PLOT_HISTOGRAM;
            style.Colors[ImGuiCol_PlotHistogramHovered] = LIGHT_PLOT_HISTOGRAM_HOVERED;
            style.Colors[ImGuiCol_TextSelectedBg]       = LIGHT_TEXT_SELECTED_BG;
            style.Colors[ImGuiCol_ModalWindowDimBg]     = LIGHT_MODAL_WINDOW_DIM_BG;
            style.WindowRounding = LIGHT_WINDOW_ROUNDING;
            style.FrameRounding  = LIGHT_FRAME_ROUNDING;
            style.GrabRounding   = LIGHT_GRAB_ROUNDING;
            style.TabRounding    = LIGHT_TAB_ROUNDING;
            break;
        }
        case 2: { // Midnight
            style.Colors[ImGuiCol_WindowBg]             = MIDNIGHT_WINDOW_BG;
            style.Colors[ImGuiCol_TitleBg]              = MIDNIGHT_TITLE_BG;
            style.Colors[ImGuiCol_TitleBgActive]        = MIDNIGHT_TITLE_BG_ACTIVE;
            style.Colors[ImGuiCol_MenuBarBg]            = ImVec4(0.08f, 0.08f, 0.12f, 1.0f);
            style.Colors[ImGuiCol_Border]               = MIDNIGHT_BORDER;
            style.Colors[ImGuiCol_BorderShadow]         = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
            style.Colors[ImGuiCol_FrameBg]              = MIDNIGHT_FRAME_BG;
            style.Colors[ImGuiCol_FrameBgHovered]       = MIDNIGHT_FRAME_BG_HOVERED;
            style.Colors[ImGuiCol_FrameBgActive]        = MIDNIGHT_FRAME_BG_ACTIVE;
            style.Colors[ImGuiCol_Button]               = MIDNIGHT_BUTTON;
            style.Colors[ImGuiCol_ButtonHovered]        = MIDNIGHT_BUTTON_HOVERED;
            style.Colors[ImGuiCol_ButtonActive]         = MIDNIGHT_BUTTON_ACTIVE;
            style.Colors[ImGuiCol_Text]                 = MIDNIGHT_TEXT;
            style.Colors[ImGuiCol_TextDisabled]         = MIDNIGHT_TEXT_DISABLED;
            style.Colors[ImGuiCol_CheckMark]            = MIDNIGHT_CHECK_MARK;
            style.Colors[ImGuiCol_ScrollbarBg]          = ImVec4(0.04f, 0.04f, 0.06f, 1.0f);
            style.Colors[ImGuiCol_ScrollbarGrab]        = MIDNIGHT_SCROLLBAR;
            style.Colors[ImGuiCol_ScrollbarGrabHovered] = MIDNIGHT_SCROLLBAR_HOVERED;
            style.Colors[ImGuiCol_ScrollbarGrabActive]  = MIDNIGHT_SCROLLBAR_ACTIVE;
            style.Colors[ImGuiCol_Tab]                  = MIDNIGHT_TAB;
            style.Colors[ImGuiCol_TabHovered]           = MIDNIGHT_TAB_HOVERED;
            style.Colors[ImGuiCol_TabActive]            = MIDNIGHT_TAB_ACTIVE;
            style.Colors[ImGuiCol_TabUnfocused]         = MIDNIGHT_WINDOW_BG;
            style.Colors[ImGuiCol_TabUnfocusedActive]   = MIDNIGHT_TAB_ACTIVE;
            style.Colors[ImGuiCol_DockingPreview]       = MIDNIGHT_DOCKING_PREVIEW;
            style.Colors[ImGuiCol_DragDropTarget]       = MIDNIGHT_DRAG_DROP_TARGET;
            style.Colors[ImGuiCol_NavHighlight]         = MIDNIGHT_NAV_HIGHLIGHT;
            style.Colors[ImGuiCol_Header]               = MIDNIGHT_HEADER;
            style.Colors[ImGuiCol_HeaderHovered]        = MIDNIGHT_HEADER_HOVERED;
            style.Colors[ImGuiCol_HeaderActive]         = MIDNIGHT_HEADER_ACTIVE;
            style.Colors[ImGuiCol_Separator]            = MIDNIGHT_SEPARATOR;
            style.Colors[ImGuiCol_SeparatorHovered]     = MIDNIGHT_FRAME_BG_HOVERED;
            style.Colors[ImGuiCol_SeparatorActive]      = MIDNIGHT_FRAME_BG_ACTIVE;
            style.Colors[ImGuiCol_ResizeGrip]           = MIDNIGHT_RESIZE_GRIP;
            style.Colors[ImGuiCol_ResizeGripHovered]    = MIDNIGHT_RESIZE_GRIP_HOVERED;
            style.Colors[ImGuiCol_ResizeGripActive]     = MIDNIGHT_RESIZE_GRIP_ACTIVE;
            style.Colors[ImGuiCol_PlotLines]            = MIDNIGHT_PLOT_LINES;
            style.Colors[ImGuiCol_PlotLinesHovered]     = MIDNIGHT_PLOT_LINES_HOVERED;
            style.Colors[ImGuiCol_PlotHistogram]        = MIDNIGHT_PLOT_HISTOGRAM;
            style.Colors[ImGuiCol_PlotHistogramHovered] = MIDNIGHT_PLOT_HISTOGRAM_HOVERED;
            style.Colors[ImGuiCol_TextSelectedBg]       = MIDNIGHT_TEXT_SELECTED_BG;
            style.Colors[ImGuiCol_ModalWindowDimBg]     = MIDNIGHT_MODAL_WINDOW_DIM_BG;
            style.WindowRounding = MIDNIGHT_WINDOW_ROUNDING;
            style.FrameRounding  = MIDNIGHT_FRAME_ROUNDING;
            style.GrabRounding   = MIDNIGHT_GRAB_ROUNDING;
            style.TabRounding    = MIDNIGHT_TAB_ROUNDING;
            break;
        }
        default:
            break;
    }

    // Force ImGui to rebuild
    ImGui::GetStyle() = currentStyle_;
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
    // Pop in reverse order of push
    // We pushed 40 style colors + 10 style vars = 50 total
    ImGui::PopStyleColor(40);
    ImGui::PopStyleVar(10);
}

ImVec4 ThemeSystem::getFrameTimeColor(float frameTimeMs) const {
    if (frameTimeMs < 16.67f) {
        return ImVec4(0.3f, 1.0f, 0.3f, 1.0f);  // green: >60fps
    } else if (frameTimeMs < 33.33f) {
        return ImVec4(1.0f, 0.85f, 0.0f, 1.0f);  // yellow: 30-60fps
    } else {
        return ImVec4(1.0f, 0.3f, 0.3f, 1.0f);  // red: <30fps
    }
}

ImVec4 ThemeSystem::getAccentColor() const {
    switch (currentPreset_) {
        case 0: return DARK_CHECK_MARK;
        case 1: return LIGHT_CHECK_MARK;
        case 2: return MIDNIGHT_CHECK_MARK;
        default: return DARK_CHECK_MARK;
    }
}

ImVec4 ThemeSystem::getTextColor() const {
    switch (currentPreset_) {
        case 0: return DARK_TEXT;
        case 1: return LIGHT_TEXT;
        case 2: return MIDNIGHT_TEXT;
        default: return DARK_TEXT;
    }
}

ImVec4 ThemeSystem::getDisabledColor() const {
    switch (currentPreset_) {
        case 0: return DARK_TEXT_DISABLED;
        case 1: return LIGHT_TEXT_DISABLED;
        case 2: return MIDNIGHT_TEXT_DISABLED;
        default: return DARK_TEXT_DISABLED;
    }
}

ImVec4 ThemeSystem::getBorderColor() const {
    switch (currentPreset_) {
        case 0: return DARK_BORDER;
        case 1: return LIGHT_BORDER;
        case 2: return MIDNIGHT_BORDER;
        default: return DARK_BORDER;
    }
}

ImVec4 ThemeSystem::getSeparatorColor() const {
    switch (currentPreset_) {
        case 0: return DARK_SEPARATOR;
        case 1: return LIGHT_SEPARATOR;
        case 2: return MIDNIGHT_SEPARATOR;
        default: return DARK_SEPARATOR;
    }
}

ImVec4 ThemeSystem::getColor(ImGuiCol idx) const {
    // Read from current ImGui style (theme already applied)
    ImU32 col = ImGui::GetColorU32(idx);
    return ImVec4(
        ((col >> 0) & 0xFF) / 255.0f,
        ((col >> 8) & 0xFF) / 255.0f,
        ((col >> 16) & 0xFF) / 255.0f,
        ((col >> 24) & 0xFF) / 255.0f
    );
}

}  // namespace engine
