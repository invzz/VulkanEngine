#ifndef EDITOR_UI_VIEWPORT_TOOLBAR_HPP
#define EDITOR_UI_VIEWPORT_TOOLBAR_HPP

#include <imgui.h>

namespace engine {

    struct FrameInfo;

    class ViewportToolbar {
       public:
        static bool render(FrameInfo& frameInfo, const ImVec2& topLeft, const ImVec2& viewportSize);
    };

}  // namespace engine

#endif
