#ifndef VULKANENGINE_INCLUDE_ENGINE_GRAPHICS_DEVICEMEMORY_HPP
#define VULKANENGINE_INCLUDE_ENGINE_GRAPHICS_DEVICEMEMORY_HPP

#include <vulkan/vulkan.h>

#include <cstdint>
#include <vector>

namespace engine {
    class Device;

    class DeviceMemory {
       public:
        explicit DeviceMemory(Device& device);

        [[nodiscard]] uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags memoryPropertyFlags) const;

        void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags memoryPropertyFlags, VkBuffer& buffer, VkDeviceMemory& bufferMemory);

        [[nodiscard]] VkCommandBuffer beginSingleTimeCommands() const;
        void                          endSingleTimeCommands(VkCommandBuffer commandBuffer) const;

        static void copyBuffer(VkCommandBuffer commandBuffer, VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size, VkPipelineStageFlags dstStageMask, VkAccessFlags dstAccessMask);

        void copyBufferImmediate(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size, VkPipelineStageFlags dstStageMask, VkAccessFlags dstAccessMask) const;

        void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height, uint32_t layerCount) const;

        void copyBufferToImage(VkBuffer buffer, VkImage image, const std::vector<VkBufferImageCopy>& regions, VkImageLayout imageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) const;

        void copyImageToBuffer(VkImage            image,
            VkBuffer                              buffer,
            const std::vector<VkBufferImageCopy>& regions,
            VkImageLayout                         srcImageLayout   = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            VkImageLayout                         finalImageLayout = VK_IMAGE_LAYOUT_UNDEFINED) const;

        void createImageWithInfo(const VkImageCreateInfo& imageInfo, VkMemoryPropertyFlags memoryPropertyFlags, VkImage& image, VkDeviceMemory& imageMemory) const;

       private:
        Device& device;
    };

}  // namespace engine

#endif
