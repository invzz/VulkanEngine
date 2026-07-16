#ifndef EDITOR_UI_VIEWPORT_PANEL_HPP
#define EDITOR_UI_VIEWPORT_PANEL_HPP
#include <imgui.h>

#include <functional>

#include "Editor/ui/UIPanel.hpp"
#include "vulkan/vulkan.h"
namespace engine {
    class Viewport;
    class Window;
    struct FrameInfo;
    /**
     * @brief Viewport panel that displays the scene preview via ImGui::Image.
     *
     * Supports two explicit modes:
     *  - Picking (default): normal cursor; click on the viewport image selects objects.
     *  - Navigation: disabled/locked cursor; WASD + mouse moves the camera.
     *
     * Mode is kept in EditorState.viewportSettings.mode and surfaced through
     * FrameInfo.viewportMode. The panel fills FrameInfo.viewportMousePos
     * and FrameInfo.viewportMouseClicked for PickingSystem.
     *
     * Navigation can be entered by:
     *  - Right-clicking inside the viewport (Blender-style).
     *  - Toggling mode from the toolbar.
     * It can be exited by pressing ESC or toggling from the toolbar.
     */
    class ViewportPanel : public UIPanel {
       public:
        ViewportPanel();
        ~ViewportPanel() override = default;
        /** Set the Viewport to display. */
        void setViewport(Viewport* viewport, VkExtent2D extent);
        /** Set the Window for cursor navigation control. */
        void setWindow(Window* window);
        /** Set the mouse for resetting deltas when entering/exiting navigation. */
        void setMouse(class Mouse* mouse);
        /** Called when the panel size changes — App wires FB resize here. */
        std::function<void(VkExtent2D)> onResize;
        void                            render(FrameInfo& frameInfo) override;
        [[nodiscard]] Viewport*         getViewport() const {
            return viewport_;
        }
        [[nodiscard]] VkExtent2D getExtent() const {
            return extent_;
        }
        [[nodiscard]] entt::entity getCameraEntity() const {
            return cameraEntity_;
        }
        void setCameraEntity(entt::entity entity) {
            cameraEntity_ = entity;
        }

       private:
        void         enterNavigation(FrameInfo& frameInfo);
        void         exitNavigation(FrameInfo& frameInfo);
        void         updateModeFromUI(FrameInfo& frameInfo);
        void         applyCursorState(ViewportMode mode);
        void         handlePickingInput(FrameInfo& frameInfo);
        void         handleResize();
        Viewport*    viewport_ = nullptr;
        VkExtent2D   extent_;
        Window*      window_       = nullptr;
        class Mouse* mouse_        = nullptr;
        entt::entity cameraEntity_ = entt::null;
        // Viewport image rect + hover state, captured right after ImGui::Image()
        // so picking is not confused by the toolbar/gizmo "last item" state.
        ImVec2 imageRectMin_{};
        ImVec2 imageRectMax_{};
        bool   imageHovered_ = false;
    };
}  // namespace engine
#endif
