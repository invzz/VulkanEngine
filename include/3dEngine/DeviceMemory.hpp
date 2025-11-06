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
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags memoryPropertyFlags) const;

    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags memoryPropertyFlags, VkBuffer& buffer, VkDeviceMemory& bufferMemory);

    VkCommandBuffer beginSingleTimeCommands() const;
    void            endSingleTimeCommands(VkCommandBuffer commandBuffer) const;

    void copyBuffer(VkCommandBuffer      commandBuffer,
                    VkBuffer             srcBuffer,
                    VkBuffer             dstBuffer,
                    VkDeviceSize         size,
                    VkPipelineStageFlags dstStageMask,
                    VkAccessFlags        dstAccessMask) const;

    void copyBufferImmediate(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size, VkPipelineStageFlags dstStageMask, VkAccessFlags dstAccessMask) const;

    void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height, uint32_t layerCount) const;

    void createImageWithInfo(const VkImageCreateInfo& imageInfo, VkMemoryPropertyFlags memoryPropertyFlags, VkImage& image, VkDeviceMemory& imageMemory) const;

  private:
    Device& device;
  };

} // namespace engine
