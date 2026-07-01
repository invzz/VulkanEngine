#pragma once

#include <entt/entt.hpp>

namespace engine {

    /**
     * EditorState — editor-only UI toggles and selection.
     * Separated from EngineState to keep runtime concerns clean.
     * For a headless/server build, this can be omitted entirely.
     */
    enum class ViewportMode : uint8_t { Picking,
        Navigation };

    struct ViewportSettings {
        ViewportMode mode = ViewportMode::Picking;
    };

    struct EditorState {
        entt::entity selectedEntity = entt::null;

        bool showSkybox             = false;
        bool showGrid               = false;
        bool showDebugObjects       = false;
        bool showColliderWireframes = false;
        bool debugMode              = false;
        bool physicsRunning         = false;
        bool solidGround            = true;

        ViewportSettings viewportSettings{};

        // Gizmo state — values match ImGuizmo::OPERATION / ImGuizmo::MODE.
        // Stored as raw int so EditorState has no dependency on ImGuizmo.
        int  gizmoOperation{0};  // ImGuizmo::TRANSLATE  (0x7)
        int  gizmoMode{1};       // ImGuizmo::WORLD      (1)
        bool gizmoEnabled{true};

        // View gizmo camera behavior:
        // true  -> orbit around selected object pivot (camera position changes)
        // false -> look/reorient in place (camera position is preserved)
        bool viewGizmoOrbitSelected{true};
    };

}  // namespace engine