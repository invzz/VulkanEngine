#ifndef VULKANENGINE_ENGINE_GRAPHICS_VIEWPORTTEXTURE_HPP
#define VULKANENGINE_ENGINE_GRAPHICS_VIEWPORTTEXTURE_HPP

#include <vulkan/vulkan.h>

#include <cstdint>

#include "Engine/Graphics/Device.hpp"

namespace engine {

    /**
     * @brief Manages a Vulkan render target texture for viewport output.
     *
     * Creates an image, image view, and sampler that can be used as:
     * - A color attachment in a render pass (rendered to)
     * - A sampled texture via ImGui::Image() (displayed in UI)
     */
    class ViewportTexture {
       public:
        ViewportTexture() = default;
        ~ViewportTexture();

        ViewportTexture(const ViewportTexture&)            = delete;
        ViewportTexture& operator=(const ViewportTexture&) = delete;

        ViewportTexture(ViewportTexture&& other) noexcept;
        ViewportTexture& operator=(ViewportTexture&& other) noexcept;

        /**
         * @brief Create the texture with the given extent.
         * @param device Vulkan device
         * @param extent Width and height of the texture
         */
        void create(Device& device, VkExtent2D extent);

        /**
         * @brief Destroy all Vulkan resources.
         * @param device Vulkan device
         */
        void destroy(Device& device);

        /**
         * @brief Resize the texture to a new extent.
         * Destroys and recreates all resources.
         * @param device Vulkan device
         * @param newExtent New width and height
         */
        void resize(Device& device, VkExtent2D newExtent);

        [[nodiscard]] VkImageView getImageView() const {
            return imageView_;
        }

        [[nodiscard]] VkImage getImage() const {
            return image_;
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

        /**
         * @brief Format used to match the offscreen color buffer for vkCmdCopyImage.
         * Must be identical to the offscreen color attachment format for the copy
         * to be valid (vkCmdCopyImage requires compatible formats).
         */
        static constexpr VkFormat kCopyFormat = VK_FORMAT_R16G16B16A16_SFLOAT;

        /**
         * @brief Get the displayable format used for the viewport texture.
         */
        static constexpr VkFormat kDisplayFormat = VK_FORMAT_B8G8R8A8_UNORM;

        /**
         * @brief Get a VkDescriptorImageInfo suitable for sampling this texture.
         */
        [[nodiscard]] VkDescriptorImageInfo getDescriptorImageInfo() const {
            VkDescriptorImageInfo info{};
            info.sampler     = sampler_;
            info.imageView   = imageView_;
            info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            return info;
        }

        /**
         * @brief Transition the image to COLOR_ATTACHMENT layout for rendering.
         * Must be called before recording render commands that use this as a color attachment.
         */
        void transitionToColorAttachment(VkCommandBuffer commandBuffer) const;

        /**
         * @brief Transition the image to TRANSFER_DST_OPTIMAL layout for copying into.
         * @param commandBuffer Command buffer to record the barrier into.
         * @param sourceLayout Optional initial layout. If SHADER_READ_ONLY_OPTIMAL (default),
         *                     assumes a normal transition. If UNDEFINED, handles initial creation.
         */
        void transitionToTransferDst(VkCommandBuffer commandBuffer, VkImageLayout sourceLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) const;

        /**
         * @brief Transition the image to SHADER_READ_ONLY_OPTIMAL layout after rendering.
         * Must be called after rendering is complete and before sampling.
         * @param commandBuffer Command buffer to record the barrier into.
         * @param sourceLayout The current layout of the image before transition.
         */
        void transitionToShaderReadOnly(VkCommandBuffer commandBuffer, VkImageLayout sourceLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) const;

       private:
        void cleanup(Device& device);
        void createSampler(Device& device);
        void createImage(Device& device, VkExtent2D extent);
        void createImageView(Device& device);

        VkImage        image_               = VK_NULL_HANDLE;
        VkDeviceMemory memory_              = VK_NULL_HANDLE;
        VkImageView    imageView_           = VK_NULL_HANDLE;
        VkSampler      sampler_             = VK_NULL_HANDLE;
        VkFormat       format_              = VK_FORMAT_UNDEFINED;
        VkExtent2D     extent_              = {0, 0};
        uint32_t       mipLevels_           = 1;
        bool           samplerOwnedByCache_ = false;
    };

}  // namespace engine

#endif  // VULKANENGINE_ENGINE_GRAPHICS_VIEWPORTTEXTURE_HPP
