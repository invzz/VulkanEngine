#ifndef EDITOR_UI_VIEWPORT_PANEL_HPP
#define EDITOR_UI_VIEWPORT_PANEL_HPP

#include <functional>

#include "Editor/ui/UIPanel.hpp"
#include "vulkan/vulkan.h"

namespace engine {

    class Viewport;

    /**
     * @brief Viewport panel that displays the scene preview via ImGui::Image.
     *
     * The Viewport holds ImTextureIDs for the offscreen color attachment —
     * no copy, no separate texture. This panel picks the correct one based
     * on the current frame-in-flight index.
     */
    class ViewportPanel : public UIPanel {
       public:
        ViewportPanel();
        ~ViewportPanel() override = default;

        /** Set the Viewport to display. */
        void setViewport(Viewport* viewport, VkExtent2D extent);

        /** Called when the panel size changes — App wires FB resize here. */
        std::function<void(VkExtent2D)> onResize;

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
