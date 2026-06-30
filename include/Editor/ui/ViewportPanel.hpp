#ifndef EDITOR_UI_VIEWPORT_PANEL_HPP
#define EDITOR_UI_VIEWPORT_PANEL_HPP

#include "Editor/ui/UIPanel.hpp"
#include "vulkan/vulkan.h"

namespace engine {

    class Viewport;

    /**
     * @brief Viewport panel that displays the scene preview via ImGui::Image.
     *
     * The Viewport owns its HDR render target; this panel displays it
     * using the ImGui texture handle registered during Viewport::create().
     */
    class ViewportPanel : public UIPanel {
       public:
        ViewportPanel();
        ~ViewportPanel() override = default;

        /** Set the Viewport to display. */
        void setViewport(Viewport* viewport, VkExtent2D extent);

        void render(FrameInfo& frameInfo) override;

        [[nodiscard]] Viewport* getViewport() const {
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

        [[nodiscard]] bool isSeparateWindow() const override {
            return true;
        }

       private:
        void handleResize();

        Viewport*  viewport_ = nullptr;
        VkExtent2D extent_;

        entt::entity cameraEntity_ = entt::null;
    };

}  // namespace engine

#endif  // EDITOR_UI_VIEWPORT_PANEL_HPP
