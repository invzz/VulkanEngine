#include "Engine/Graphics/DeviceMemory.hpp"

#include <stdexcept>

#include "Engine/Core/Exceptions.hpp"
#include "Engine/Graphics/Device.hpp"

namespace engine {

  DeviceMemory::DeviceMemory(Device& device) : device(device) {}

  uint32_t DeviceMemory::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags memoryPropertyFlags) const
  {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(device.physicalDevice, &memProperties);
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
    {
      if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & memoryPropertyFlags) == memoryPropertyFlags)
      {
        return i;
      }
    }

    throw engine::RuntimeException("failed to find suitable memory type!");
  }

  void DeviceMemory::createBuffer(VkDeviceSize          size,
                                  VkBufferUsageFlags    usage,
                                  VkMemoryPropertyFlags memoryPropertyFlags,
                                  VkBuffer&             buffer,
                                  VkDeviceMemory&       bufferMemory)
  {
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size        = size;
    bufferInfo.usage       = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device.device_, &bufferInfo, nullptr, &buffer) != VK_SUCCESS)
    {
      throw engine::RuntimeException("failed to create vertex buffer!");
    }

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device.device_, buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize  = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, memoryPropertyFlags);

    VkMemoryAllocateFlagsInfo allocFlagsInfo{};
    if (usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT)
    {
      allocFlagsInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
      allocFlagsInfo.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;
      allocInfo.pNext      = &allocFlagsInfo;
    }

    if (vkAllocateMemory(device.device_, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS)
    {
      throw engine::RuntimeException("failed to allocate vertex buffer memory!");
    }

    vkBindBufferMemory(device.device_, buffer, bufferMemory, 0);
  }

  VkCommandBuffer DeviceMemory::beginSingleTimeCommands() const
  {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool        = device.commandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(device.device_, &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);
    return commandBuffer;
  }

  void DeviceMemory::endSingleTimeCommands(VkCommandBuffer commandBuffer) const
  {
    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers    = &commandBuffer;

    vkQueueSubmit(device.graphicsQueue_, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(device.graphicsQueue_);

    vkFreeCommandBuffers(device.device_, device.commandPool, 1, &commandBuffer);
  }

  void DeviceMemory::copyBuffer(VkCommandBuffer      commandBuffer,
                                VkBuffer             srcBuffer,
                                VkBuffer             dstBuffer,
                                VkDeviceSize         size,
                                VkPipelineStageFlags dstStageMask,
                                VkAccessFlags        dstAccessMask) const
  {
    VkBufferCopy copyRegion{};
    copyRegion.srcOffset = 0; // Optional
    copyRegion.dstOffset = 0; // Optional
    copyRegion.size      = size;
    vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

    VkBufferMemoryBarrier barrier{};
    barrier.sType               = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    barrier.srcAccessMask       = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask       = dstAccessMask;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.buffer              = dstBuffer;
    barrier.offset              = 0;
    barrier.size                = VK_WHOLE_SIZE;

    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, dstStageMask, 0, 0, nullptr, 1, &barrier, 0, nullptr);
  }

  void DeviceMemory::copyBufferImmediate(VkBuffer             srcBuffer,
                                         VkBuffer             dstBuffer,
                                         VkDeviceSize         size,
                                         VkPipelineStageFlags dstStageMask,
                                         VkAccessFlags        dstAccessMask) const
  {
    VkCommandBuffer commandBuffer = beginSingleTimeCommands();
    copyBuffer(commandBuffer, srcBuffer, dstBuffer, size, dstStageMask, dstAccessMask);
    endSingleTimeCommands(commandBuffer);
  }

  void DeviceMemory::copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height, uint32_t layerCount) const
  {
    VkCommandBuffer commandBuffer = beginSingleTimeCommands();

    VkBufferImageCopy region{};
    region.bufferOffset      = 0;
    region.bufferRowLength   = 0;
    region.bufferImageHeight = 0;

    region.imageSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel       = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount     = layerCount;

    region.imageOffset = {0, 0, 0};
    region.imageExtent = {width, height, 1};

    vkCmdCopyBufferToImage(commandBuffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
    endSingleTimeCommands(commandBuffer);
  }

  void DeviceMemory::createImageWithInfo(const VkImageCreateInfo& imageInfo,
                                         VkMemoryPropertyFlags    memoryPropertyFlags,
                                         VkImage&                 image,
                                         VkDeviceMemory&          imageMemory) const
  {
    if (vkCreateImage(device.device_, &imageInfo, nullptr, &image) != VK_SUCCESS)
    {
      throw engine::RuntimeException("failed to create image!");
    }

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(device.device_, image, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize  = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, memoryPropertyFlags);

    if (vkAllocateMemory(device.device_, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS)
    {
      throw engine::RuntimeException("failed to allocate image memory!");
    }

    if (vkBindImageMemory(device.device_, image, imageMemory, 0) != VK_SUCCESS)
    {
      throw engine::RuntimeException("failed to bind image memory!");
    }
  }

} // namespace engine
