#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>
#include <vector>

namespace engine {
    class Device;

    class DeviceMemory
    {
      public:
        explicit DeviceMemory(Device& device);

        // Memory & buffer helper functions
        uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags memoryPropertyFlags);

        void createBuffer(VkDeviceSize          size,
                          VkBufferUsageFlags    usage,
                          VkMemoryPropertyFlags memoryPropertyFlags,
                          VkBuffer&             buffer,
                          VkDeviceMemory&       bufferMemory);

        VkCommandBuffer beginSingleTimeCommands();
        void            endSingleTimeCommands(VkCommandBuffer commandBuffer);

        void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);

        void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height, uint32_t layerCount);

        void createImageWithInfo(const VkImageCreateInfo& imageInfo,
                                 VkMemoryPropertyFlags    memoryPropertyFlags,
                                 VkImage&                 image,
                                 VkDeviceMemory&          imageMemory);

      private:
        Device& device;
    };

} // namespace engine
