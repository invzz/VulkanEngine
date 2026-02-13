#include "Engine/Graphics/DeviceMemory.hpp"

#include <cstdint>
#include <iostream>
#include <thread>

#include "Engine/Core/Exceptions.hpp"
#include "Engine/Graphics/Device.hpp"
#include "vulkan/vulkan_core.h"

namespace engine {

DeviceMemory::DeviceMemory(Device& device) : device(device) {}

uint32_t DeviceMemory::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags memoryPropertyFlags) const {
  VkPhysicalDeviceMemoryProperties memProperties;
  vkGetPhysicalDeviceMemoryProperties(device.physicalDevice, &memProperties);
  for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
    if (((typeFilter & (1 << i)) != 0u) && (memProperties.memoryTypes[i].propertyFlags & memoryPropertyFlags) == memoryPropertyFlags) {
      return i;
    }
  }

  throw engine::RuntimeException("failed to find suitable memory type!");
}

void DeviceMemory::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags memoryPropertyFlags, VkBuffer& buffer, VkDeviceMemory& bufferMemory) {
  VkBufferCreateInfo bufferInfo{};
  bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  bufferInfo.size = size;
  bufferInfo.usage = usage;
  bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  if (vkCreateBuffer(device.device_, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
    throw engine::RuntimeException("failed to create vertex buffer!");
  }

  VkMemoryRequirements memRequirements;
  vkGetBufferMemoryRequirements(device.device_, buffer, &memRequirements);

  VkMemoryAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  allocInfo.allocationSize = memRequirements.size;
  allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, memoryPropertyFlags);

  VkMemoryAllocateFlagsInfo allocFlagsInfo{};
  if ((usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) != 0u) {
    allocFlagsInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
    allocFlagsInfo.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;
    allocInfo.pNext = &allocFlagsInfo;
  }

  if (vkAllocateMemory(device.device_, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
    throw engine::RuntimeException("failed to allocate vertex buffer memory!");
  }

  vkBindBufferMemory(device.device_, buffer, bufferMemory, 0);
}

VkCommandBuffer DeviceMemory::beginSingleTimeCommands() const {
  // Delegate to Device implementation which creates a temporary command pool
  // per-call so worker threads don't share the main command pool.
  return device.beginSingleTimeCommands();
}

void DeviceMemory::endSingleTimeCommands(VkCommandBuffer commandBuffer) const {
  // Delegate to Device implementation which frees the temp pool when done.
  device.endSingleTimeCommands(commandBuffer);
}

void DeviceMemory::copyBuffer(VkCommandBuffer commandBuffer, VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size, VkPipelineStageFlags dstStageMask, VkAccessFlags dstAccessMask) {
  VkBufferCopy copyRegion{};
  copyRegion.srcOffset = 0;  // Optional
  copyRegion.dstOffset = 0;  // Optional
  copyRegion.size = size;
  vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

  VkBufferMemoryBarrier barrier{};
  barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
  barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  barrier.dstAccessMask = dstAccessMask;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.buffer = dstBuffer;
  barrier.offset = 0;
  barrier.size = VK_WHOLE_SIZE;

  vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, dstStageMask, 0, 0, nullptr, 1, &barrier, 0, nullptr);
}

void DeviceMemory::copyBufferImmediate(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size, VkPipelineStageFlags dstStageMask, VkAccessFlags dstAccessMask) const {
  VkCommandBuffer commandBuffer = beginSingleTimeCommands();
  copyBuffer(commandBuffer, srcBuffer, dstBuffer, size, dstStageMask, dstAccessMask);
  endSingleTimeCommands(commandBuffer);
}

void DeviceMemory::copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height, uint32_t layerCount) const {
  VkCommandBuffer commandBuffer = beginSingleTimeCommands();

  VkBufferImageCopy region{};
  region.bufferOffset = 0;
  region.bufferRowLength = 0;
  region.bufferImageHeight = 0;

  region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  region.imageSubresource.mipLevel = 0;
  region.imageSubresource.baseArrayLayer = 0;
  region.imageSubresource.layerCount = layerCount;

  region.imageOffset = {0, 0, 0};
  region.imageExtent = {width, height, 1};

  vkCmdCopyBufferToImage(commandBuffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
  endSingleTimeCommands(commandBuffer);
}

void DeviceMemory::copyBufferToImage(VkBuffer buffer, VkImage image, const std::vector<VkBufferImageCopy>& regions, VkImageLayout imageLayout) const {
  VkCommandBuffer commandBuffer = beginSingleTimeCommands();
  vkCmdCopyBufferToImage(commandBuffer, buffer, image, imageLayout, static_cast<uint32_t>(regions.size()), regions.data());
  endSingleTimeCommands(commandBuffer);
}

void DeviceMemory::copyImageToBuffer(VkImage image, VkBuffer buffer, const std::vector<VkBufferImageCopy>& regions, VkImageLayout srcImageLayout, VkImageLayout finalImageLayout) const {
  std::cerr << "[DeviceMemory] copyImageToBuffer - begin thread=" << std::this_thread::get_id() << " regions=" << regions.size() << "\n";

  // Basic defensive check
  if (srcImageLayout == VK_IMAGE_LAYOUT_UNDEFINED) {
    throw engine::RuntimeException("copyImageToBuffer called with VK_IMAGE_LAYOUT_UNDEFINED as source layout");
  }

  VkCommandBuffer commandBuffer = beginSingleTimeCommands();

  // If the image is not already in TRANSFER_SRC, transition it for the copy.
  if (srcImageLayout != VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) {
    for (const auto& r : regions) {
      VkImageMemoryBarrier preBarrier{};
      preBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
      preBarrier.oldLayout = srcImageLayout;
      preBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
      preBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
      preBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
      preBarrier.image = image;
      preBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      preBarrier.subresourceRange.baseMipLevel = r.imageSubresource.mipLevel;
      preBarrier.subresourceRange.levelCount = 1;
      preBarrier.subresourceRange.baseArrayLayer = r.imageSubresource.baseArrayLayer;
      preBarrier.subresourceRange.layerCount = r.imageSubresource.layerCount;

      // Choose masks conservatively based on common layouts
      if (srcImageLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        preBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
      } else if (srcImageLayout == VK_IMAGE_LAYOUT_GENERAL) {
        preBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;
      } else {
        preBarrier.srcAccessMask = 0;
      }
      preBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

      vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &preBarrier);
    }
  }

  // Perform the copy
  vkCmdCopyImageToBuffer(commandBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, buffer, static_cast<uint32_t>(regions.size()), regions.data());

  // If requested, transition the image back to a final layout (e.g. SHADER_READ) after the copy
  if (finalImageLayout != VK_IMAGE_LAYOUT_UNDEFINED) {
    for (const auto& r : regions) {
      VkImageMemoryBarrier postBarrier{};
      postBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
      postBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
      postBarrier.newLayout = finalImageLayout;
      postBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
      postBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
      postBarrier.image = image;
      postBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      postBarrier.subresourceRange.baseMipLevel = r.imageSubresource.mipLevel;
      postBarrier.subresourceRange.levelCount = 1;
      postBarrier.subresourceRange.baseArrayLayer = r.imageSubresource.baseArrayLayer;
      postBarrier.subresourceRange.layerCount = r.imageSubresource.layerCount;

      postBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
      if (finalImageLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        postBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &postBarrier);
      } else {
        // Generic transition: use transfer -> top-of-pipe to be conservative
        postBarrier.dstAccessMask = 0;
        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &postBarrier);
      }
    }
  }

  endSingleTimeCommands(commandBuffer);

  // Conservative sync: ensure the transfer and any layout transitions are visible
  // to subsequent submissions (helps avoid ordering issues when callers immediately
  // record/submit commands that assume the image is in `finalImageLayout`). This
  // is intentionally conservative and only performed when a final layout was
  // requested; we can optimize later with finer-grained semaphores if needed.
  if (finalImageLayout != VK_IMAGE_LAYOUT_UNDEFINED) {
    vkDeviceWaitIdle(device.device_);
  }

  std::cerr << "[DeviceMemory] copyImageToBuffer - complete thread=" << std::this_thread::get_id() << "\n";
}

void DeviceMemory::createImageWithInfo(const VkImageCreateInfo& imageInfo, VkMemoryPropertyFlags memoryPropertyFlags, VkImage& image, VkDeviceMemory& imageMemory) const {
  if (vkCreateImage(device.device_, &imageInfo, nullptr, &image) != VK_SUCCESS) {
    throw engine::RuntimeException("failed to create image!");
  }

  VkMemoryRequirements memRequirements;
  vkGetImageMemoryRequirements(device.device_, image, &memRequirements);

  VkMemoryAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  allocInfo.allocationSize = memRequirements.size;
  allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, memoryPropertyFlags);

  if (vkAllocateMemory(device.device_, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS) {
    throw engine::RuntimeException("failed to allocate image memory!");
  }

  if (vkBindImageMemory(device.device_, image, imageMemory, 0) != VK_SUCCESS) {
    throw engine::RuntimeException("failed to bind image memory!");
  }
}

}  // namespace engine
