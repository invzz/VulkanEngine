#ifndef VULKANENGINE_ENGINE_GRAPHICS_VIEWPORT_HPP
#define VULKANENGINE_ENGINE_GRAPHICS_VIEWPORT_HPP

#include <vulkan/vulkan.h>

#include <imgui.h>

#include "Engine/Graphics/Device.hpp"

namespace engine {

    /**
     * @brief Viewport render target that owns its HDR texture for scene output
     *        and ImGui display. Replaces ViewportTexture + ViewportDisplay.
     *
     * The Viewport owns:
     *  - An HDR render target (R16G16B16A16_SFLOAT) for offscreen scene rendering
     *  - A VkSampler for ImGui display
     *  - An ImTextureID registered with ImGui Vulkan backend
     *
     * Scene output is copied from offscreen buffer to this target via vkCmdCopyImage.
     * The ViewportPanel displays the texture via ImGui::Image() in the CompositionPass.
     */
    class Viewport {
       public:
        Viewport() = default;
        ~Viewport();

        Viewport(const Viewport&)            = delete;
        Viewport& operator=(const Viewport&) = delete;
        Viewport(Viewport&& other) noexcept;
        Viewport& operator=(Viewport&& other) noexcept;

        /**
         * @brief Create the viewport with the given extent and register with ImGui.
         */
        void create(Device& device, VkExtent2D extent);

        /**
         * @brief Resize the viewport. Destroys and recreates the HDR target.
         */
        void resize(Device& device, VkExtent2D newExtent);

        /**
         * @brief Destroy all Vulkan resources (call before device teardown).
         */
        void destroy();

        // --- HDR Render Target Access ---

        [[nodiscard]] VkImage getImage() const {
            return image_;
        }
        [[nodiscard]] VkImageView getImageView() const {
            return imageView_;
        }
        [[nodiscard]] VkSampler getSampler() const {
            return sampler_;
        }
        [[nodiscard]] VkExtent2D getExtent() const {
            return extent_;
        }
        [[nodiscard]] VkFormat getFormat() const {
            return format_;
        }

        // --- ImGui Display ---

        [[nodiscard]] ImTextureID getImTextureID() const {
            return imTextureID_;
        }

        // --- Layout Transitions ---

        /// Transition from SHADER_READ_ONLY to TRANSFER_DST_OPTIMAL.
        void transitionToTransferDst(VkCommandBuffer cmd,
            VkImageLayout                            sourceLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) const;

        /// Transition to SHADER_READ_ONLY_OPTIMAL (for ImGui sampling).
        void transitionToShaderReadOnly(VkCommandBuffer cmd,
            VkImageLayout                               sourceLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) const;

        /// Get a descriptor image info for sampling.
        [[nodiscard]] VkDescriptorImageInfo getDescriptorImageInfo() const;

       private:
        void createImage(Device& device, VkExtent2D extent);
        void createImageView(Device& device);
        void createSampler(Device& device);
        void registerWithImGui(Device& device);

        VkImage        image_       = VK_NULL_HANDLE;
        VkDeviceMemory memory_      = VK_NULL_HANDLE;
        VkImageView    imageView_   = VK_NULL_HANDLE;
        VkSampler      sampler_     = VK_NULL_HANDLE;
        VkFormat       format_      = VK_FORMAT_UNDEFINED;
        VkExtent2D     extent_      = {0, 0};
        ImTextureID    imTextureID_ = (ImTextureID) nullptr;

        Device* device_ = nullptr;  // cached for destroy
    };

}  // namespace engine

#endif  // VULKANENGINE_ENGINE_GRAPHICS_VIEWPORT_HPP
