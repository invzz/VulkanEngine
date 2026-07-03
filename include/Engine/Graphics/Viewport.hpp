#ifndef VULKANENGINE_ENGINE_GRAPHICS_VIEWPORT_HPP
#define VULKANENGINE_ENGINE_GRAPHICS_VIEWPORT_HPP

#include <vulkan/vulkan.h>

#include <imgui.h>

#include <array>

#include "Engine/Graphics/Device.hpp"

namespace engine {

    class Renderer;

    /**
     * @brief Viewport that displays the offscreen framebuffer's color attachment
     *        directly in ImGui — no copy, no separate texture. Unreal-style:
     *        the render target IS the viewport texture.
     *
     * On create(), registers each frame-in-flight color image with ImGui.
     * On resize(), triggers offscreen framebuffer resize and re-registers.
     * ViewportPanel calls getImTextureID(frameIndex) to pick the right image.
     */
    class Viewport {
       public:
        Viewport()  = default;
        ~Viewport() = default;

        Viewport(const Viewport&)            = delete;
        Viewport& operator=(const Viewport&) = delete;

        /**
         * @brief Register offscreen color images with ImGui Vulkan backend.
         * @param device The Vulkan device (used for ImGui_ImplVulkan_AddTexture).
         * @param renderer The Renderer whose offscreen color attachments to register.
         */
        void create(Device& device, Renderer& renderer);

        /**
         * @brief Resize the offscreen framebuffer and re-register color images.
         */
        void resize(Device& device, Renderer& renderer, VkExtent2D newExtent);

        /**
         * @brief Get the ImTextureID for the current frame's offscreen color image.
         */
        [[nodiscard]] ImTextureID getImTextureID(int frameIndex) const {
            return imTextureIDs_[static_cast<size_t>(frameIndex)];
        }

       private:
        void registerAllFrames(Device& device, Renderer& renderer);

        std::array<ImTextureID, 4> imTextureIDs_{};
    };

}  // namespace engine

#endif
