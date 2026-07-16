#pragma once

#include <imgui.h>

namespace engine::ui {

    /// Per-element accent colors for the scene tree / panel widgets.
    /// Kept separate from engine::Theme (ThemeSystem) which drives window
    /// chrome; these are semantic entity-type colors used by the tree rows.
    namespace Theme {

        inline constexpr ImVec4 Model    = {0.4f, 0.8f, 1.0f, 1.0f};
        inline constexpr ImVec4 Light    = {1.0f, 1.0f, 0.0f, 1.0f};
        inline constexpr ImVec4 SubMesh  = {0.55f, 0.85f, 1.0f, 1.0f};
        inline constexpr ImVec4 Instance = {0.6f, 0.9f, 0.7f, 1.0f};
        inline constexpr ImVec4 Camera   = {1.0f, 1.0f, 1.0f, 1.0f};
        inline constexpr ImVec4 Info     = {0.4f, 0.8f, 1.0f, 1.0f};

    }  // namespace Theme

}  // namespace engine::ui
