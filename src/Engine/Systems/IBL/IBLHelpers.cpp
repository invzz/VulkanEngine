#include "Engine/Systems/IBL/IBLHelpers.hpp"

#include <stdexcept>

#include "Engine/Graphics/Device.hpp"
#include "Engine/Graphics/DeviceMemory.hpp"

namespace engine::ibl_detail {

void createImage(Device& device,
    uint32_t width,
    uint32_t height,
    uint32_t mipLevels,
    VkFormat format,
    VkImageTiling tiling,
    VkImageUsageFlags usage,
    VkMemoryPropertyFlags properties,
    VkImage& image,
    VkDeviceMemory& imageMemory,
    uint32_t arrayLayers,
    VkImageCreateFlags flags) {
  VkImageCreateInfo imageInfo{};
  imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  imageInfo.imageType = VK_IMAGE_TYPE_2D;
  imageInfo.extent.width = width;
  imageInfo.extent.height = height;
  imageInfo.extent.depth = 1;
  imageInfo.mipLevels = mipLevels;
  imageInfo.arrayLayers = arrayLayers;
  imageInfo.format = format;
  imageInfo.tiling = tiling;
  imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  imageInfo.usage = usage;
  imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
  imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  imageInfo.flags = flags;

  device.getMemory().createImageWithInfo(imageInfo, properties, image, imageMemory);
}

VkImageView createImageView(Device& device,
    VkImage image,
    VkFormat format,
    VkImageAspectFlags aspectFlags,
    uint32_t mipLevels,
    VkImageViewType viewType,
    uint32_t baseMipLevel,
    uint32_t layerCount,
    uint32_t baseArrayLayer) {
  VkImageViewCreateInfo viewInfo{};
  viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  viewInfo.image = image;
  viewInfo.viewType = viewType;
  viewInfo.format = format;
  viewInfo.subresourceRange.aspectMask = aspectFlags;
  viewInfo.subresourceRange.baseMipLevel = baseMipLevel;
  viewInfo.subresourceRange.levelCount = mipLevels;
  viewInfo.subresourceRange.baseArrayLayer = baseArrayLayer;
  viewInfo.subresourceRange.layerCount = layerCount;

  VkImageView imageView;
  if (vkCreateImageView(device.device(), &viewInfo, nullptr, &imageView) != VK_SUCCESS) {
    throw std::runtime_error("failed to create texture image view!");
  }
  return imageView;
}

void transitionImageLayout(Device& device, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t mipLevels, uint32_t layerCount) {
  VkCommandBuffer commandBuffer = device.getMemory().beginSingleTimeCommands();

  VkImageMemoryBarrier barrier{};
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.oldLayout = oldLayout;

  barrier.newLayout = newLayout;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.image = image;
  barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  barrier.subresourceRange.baseMipLevel = 0;
  barrier.subresourceRange.levelCount = mipLevels;
  barrier.subresourceRange.baseArrayLayer = 0;
  barrier.subresourceRange.layerCount = layerCount;

  VkPipelineStageFlags sourceStage;
  VkPipelineStageFlags destinationStage;

  if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
  } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
  } else if (oldLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) {
    // Readback path for VTEX export.
    barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    sourceStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
  } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
  } else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) {
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    destinationStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  } else if (oldLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
    barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    sourceStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
  } else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_GENERAL) {
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    destinationStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
  } else if (oldLayout == VK_IMAGE_LAYOUT_GENERAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
    barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    sourceStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
  } else {
    throw std::invalid_argument("unsupported layout transition!");
  }

  vkCmdPipelineBarrier(commandBuffer, sourceStage, destinationStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);

  device.getMemory().endSingleTimeCommands(commandBuffer);
}

void deferDestroySampler(Device& device, VkSampler& sampler) {
  if (sampler == VK_NULL_HANDLE) return;
  VkSampler toDestroy = sampler;
  sampler = VK_NULL_HANDLE;
  device.deferDestroy([toDestroy](VkDevice dev) { vkDestroySampler(dev, toDestroy, nullptr); });
}

void deferDestroyImageView(Device& device, VkImageView& view) {
  if (view == VK_NULL_HANDLE) return;
  VkImageView toDestroy = view;
  view = VK_NULL_HANDLE;
  device.deferDestroy([toDestroy](VkDevice dev) { vkDestroyImageView(dev, toDestroy, nullptr); });
}

void deferDestroyImage(Device& device, VkImage& image) {
  if (image == VK_NULL_HANDLE) return;
  VkImage toDestroy = image;
  image = VK_NULL_HANDLE;
  device.deferDestroy([toDestroy](VkDevice dev) { vkDestroyImage(dev, toDestroy, nullptr); });
}

void deferFreeMemory(Device& device, VkDeviceMemory& mem) {
  if (mem == VK_NULL_HANDLE) return;
  VkDeviceMemory toFree = mem;
  mem = VK_NULL_HANDLE;
  device.deferDestroy([toFree](VkDevice dev) { vkFreeMemory(dev, toFree, nullptr); });
}

void deferDestroyPipeline(Device& device, VkPipeline& pipeline) {
  if (pipeline == VK_NULL_HANDLE) return;
  VkPipeline toDestroy = pipeline;
  pipeline = VK_NULL_HANDLE;
  device.deferDestroy([toDestroy](VkDevice dev) { vkDestroyPipeline(dev, toDestroy, nullptr); });
}

void deferDestroyPipelineLayout(Device& device, VkPipelineLayout& layout) {
  if (layout == VK_NULL_HANDLE) return;
  VkPipelineLayout toDestroy = layout;
  layout = VK_NULL_HANDLE;
  device.deferDestroy([toDestroy](VkDevice dev) { vkDestroyPipelineLayout(dev, toDestroy, nullptr); });
}

void deferDestroyRenderPass(Device& device, VkRenderPass& renderPass) {
  if (renderPass == VK_NULL_HANDLE) return;
  VkRenderPass toDestroy = renderPass;
  renderPass = VK_NULL_HANDLE;
  device.deferDestroy([toDestroy](VkDevice dev) { vkDestroyRenderPass(dev, toDestroy, nullptr); });
}

void deferDestroyDescriptorPool(Device& device, VkDescriptorPool& pool) {
  if (pool == VK_NULL_HANDLE) return;
  VkDescriptorPool toDestroy = pool;
  pool = VK_NULL_HANDLE;
  device.deferDestroy([toDestroy](VkDevice dev) { vkDestroyDescriptorPool(dev, toDestroy, nullptr); });
}

void deferDestroyDescriptorSetLayout(Device& device, VkDescriptorSetLayout& layout) {
  if (layout == VK_NULL_HANDLE) return;
  VkDescriptorSetLayout toDestroy = layout;
  layout = VK_NULL_HANDLE;
  device.deferDestroy([toDestroy](VkDevice dev) { vkDestroyDescriptorSetLayout(dev, toDestroy, nullptr); });
}

}  // namespace engine::ibl_detail
