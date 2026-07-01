#ifndef EDITOR_UI_VIEWPORT_VIEW_GIZMO_HPP
#define EDITOR_UI_VIEWPORT_VIEW_GIZMO_HPP

#include <imgui.h>

namespace engine {

    struct FrameInfo;

    class ViewportViewGizmo {
       public:
        static void render(FrameInfo& frameInfo, const ImVec2& topLeft, const ImVec2& size);
    };

}  // namespace engine

#endif  // EDITOR_UI_VIEWPORT_VIEW_GIZMO_HPP
