#ifndef EDITOR_UI_VIEWPORT_PANEL_HPP
#define EDITOR_UI_VIEWPORT_PANEL_HPP

#include <cstdint>
#include <imgui.h>

#include "Editor/ui/UIPanel.hpp"
#include "vulkan/vulkan.h"

namespace engine {

    class ImGuiManager;
    class ViewportTexture;

    /**
     * @brief Viewport panel that displays the scene preview via ImGui::Image.
     *
     * Renders the scene to a ViewportTexture, then displays that texture
     * in the viewport panel using ImGui. Handles resize events.
     */
    class ViewportPanel : public UIPanel {
       public:
        ViewportPanel();
        ~ViewportPanel() override = default;

        /**
         * @brief Initialize the viewport panel.
         * @param imguiManager ImGui manager for texture handle creation
         * @param viewportTexture Pointer to the viewport texture to display
         * @param extent Initial extent of the viewport
         */
        void initialize(ImGuiManager& imguiManager, ViewportTexture* viewportTexture, VkExtent2D extent);

        void render(FrameInfo& frameInfo) override;

        /**
         * @brief Get the viewport texture.
         */
        ViewportTexture* getViewportTexture() const {
            return viewportTexture_;
        }

        /**
         * @brief Get the current camera entity.
         */
        entt::entity getCameraEntity() const {
            return cameraEntity_;
        }

        /**
         * @brief Set the camera entity for this viewport.
         */
        void setCameraEntity(entt::entity entity) {
            cameraEntity_ = entity;
        }

        /**
         * @brief Check if this panel should render in a separate window.
         */
        [[nodiscard]] bool isSeparateWindow() const override {
            return true;
        }

       private:
        void renderViewportContent(FrameInfo& frameInfo);
        void handleResize();

        ImGuiManager*    imguiManager_    = nullptr;
        ViewportTexture* viewportTexture_ = nullptr;
        VkExtent2D       extent_;
        int              currentFrame_ = 0;

        // Camera input state
        entt::entity cameraEntity_ = entt::null;

        // ImGui texture handle
        ImTextureID textureHandle_ = (ImTextureID) 0;
    };

}  // namespace engine

#endif  // EDITOR_UI_VIEWPORT_PANEL_HPP
