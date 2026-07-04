#pragma once
#include <vulkan/vulkan.h>

#include <cstdint>
namespace engine {
    class Device;
}
namespace engine::ibl_detail {
    void        createImage(Device&  device,
        uint32_t              width,
        uint32_t              height,
        uint32_t              mipLevels,
        VkFormat              format,
        VkImageTiling         tiling,
        VkImageUsageFlags     usage,
        VkMemoryPropertyFlags properties,
        VkImage&              image,
        VkDeviceMemory&       imageMemory,
        uint32_t              arrayLayers = 1,
        VkImageCreateFlags    flags       = 0);
    VkImageView createImageView(Device& device,
        VkImage                         image,
        VkFormat                        format,
        VkImageAspectFlags              aspectFlags,
        uint32_t                        mipLevels,
        VkImageViewType                 viewType       = VK_IMAGE_VIEW_TYPE_2D,
        uint32_t                        baseMipLevel   = 0,
        uint32_t                        layerCount     = 1,
        uint32_t                        baseArrayLayer = 0);
    void        transitionImageLayout(Device& device, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t mipLevels, uint32_t layerCount = 1);
    void        deferDestroySampler(Device& device, VkSampler& sampler);
    void        deferDestroyImageView(Device& device, VkImageView& view);
    void        deferDestroyImage(Device& device, VkImage& image);
    void        deferFreeMemory(Device& device, VkDeviceMemory& mem);
    void        deferDestroyPipeline(Device& device, VkPipeline& pipeline);
    void        deferDestroyPipelineLayout(Device& device, VkPipelineLayout& layout);
    void        deferDestroyRenderPass(Device& device, VkRenderPass& renderPass);
    void        deferDestroyDescriptorPool(Device& device, VkDescriptorPool& pool);
    void        deferDestroyDescriptorSetLayout(Device& device, VkDescriptorSetLayout& layout);
}  // namespace engine::ibl_detail
