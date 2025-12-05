#include "Engine/Resources/Texture.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <cmath>
#include <iostream>
#include <stdexcept>

#include "Engine/Core/ansi_colors.hpp"
#include "Engine/Graphics/Buffer.hpp"

namespace engine {

  Texture::Texture(Device& device, const std::string& filepath, bool srgb, bool flipY) : device_{device}
  {
    // Load image using stb_image
    int texChannels;

    if (flipY)
    {
      stbi_set_flip_vertically_on_load(true);
    }

    stbi_uc* pixels = stbi_load(filepath.c_str(), &width_, &height_, &texChannels, STBI_rgb_alpha);

    if (flipY)
    {
      stbi_set_flip_vertically_on_load(false);
    }

    if (!pixels)
    {
      throw std::runtime_error("Failed to load texture image: " + filepath);
    }

    VkDeviceSize imageSize = width_ * height_ * 4; // RGBA

    // Calculate mip levels
    mipLevels_ = static_cast<uint32_t>(std::floor(std::log2(std::max(width_, height_)))) + 1;

    // Create staging buffer
    Buffer stagingBuffer{device_,
                         1,
                         static_cast<uint32_t>(imageSize),
                         VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT};

    stagingBuffer.map();
    stagingBuffer.writeToBuffer(pixels);
    stagingBuffer.unmap();

    stbi_image_free(pixels);

    // Choose format based on whether this is an sRGB texture (color) or linear (data)
    VkFormat format = srgb ? VK_FORMAT_R8G8B8A8_SRGB : VK_FORMAT_R8G8B8A8_UNORM;

    // Create Vulkan image
    createImage(width_,
                height_,
                mipLevels_,
                format,
                VK_IMAGE_TILING_OPTIMAL,
                VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    // Transition image layout and copy buffer to image
    transitionImageLayout(image_, format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, mipLevels_);
    copyBufferToImage(stagingBuffer.getBuffer(), image_, static_cast<uint32_t>(width_), static_cast<uint32_t>(height_));

    // Generate mipmaps (this also transitions to SHADER_READ_ONLY_OPTIMAL)
    generateMipmaps(image_, format, width_, height_, mipLevels_);

    // Create image view and sampler
    createImageView(format);
    createSampler();

    std::cout << "[" << GREEN << "Texture" << RESET << "] Loaded: " << filepath << " (" << width_ << "x" << height_ << ", " << mipLevels_ << " mips)"
              << std::endl;
  }

  Texture::~Texture()
  {
    if (sampler_ != VK_NULL_HANDLE)
    {
      vkDestroySampler(device_.device(), sampler_, nullptr);
    }
    if (imageView_ != VK_NULL_HANDLE)
    {
      vkDestroyImageView(device_.device(), imageView_, nullptr);
    }
    if (image_ != VK_NULL_HANDLE)
    {
      vkDestroyImage(device_.device(), image_, nullptr);
    }
    if (imageMemory_ != VK_NULL_HANDLE)
    {
      vkFreeMemory(device_.device(), imageMemory_, nullptr);
    }
  }

  // Private constructor for creating textures from memory
  Texture::Texture(Device& device, const unsigned char* pixels, int width, int height, VkFormat format) : device_{device}, width_{width}, height_{height}
  {
    VkDeviceSize imageSize = width_ * height_ * 4; // RGBA
    mipLevels_             = 1;                    // No mipmaps for default textures

    // Create staging buffer
    Buffer stagingBuffer{device_,
                         1,
                         static_cast<uint32_t>(imageSize),
                         VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT};

    stagingBuffer.map();
    stagingBuffer.writeToBuffer((void*)pixels);
    stagingBuffer.unmap();

    // Create Vulkan image
    createImage(width_,
                height_,
                mipLevels_,
                format,
                VK_IMAGE_TILING_OPTIMAL,
                VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    // Transition image layout and copy buffer to image
    transitionImageLayout(image_, format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, mipLevels_);
    copyBufferToImage(stagingBuffer.getBuffer(), image_, static_cast<uint32_t>(width_), static_cast<uint32_t>(height_));
    transitionImageLayout(image_, format, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, mipLevels_);

    // Create image view and sampler
    createImageView(format);
    createSampler();
  }

  std::shared_ptr<Texture> Texture::createWhiteTexture(Device& device)
  {
    unsigned char whitePixel[4] = {255, 255, 255, 255};
    return std::shared_ptr<Texture>(new Texture(device, whitePixel, 1, 1, VK_FORMAT_R8G8B8A8_UNORM));
  }

  std::shared_ptr<Texture> Texture::createNormalTexture(Device& device)
  {
    // Flat normal pointing up in tangent space: (0, 0, 1) -> (128, 128, 255) in RGB
    unsigned char normalPixel[4] = {128, 128, 255, 255};
    return std::shared_ptr<Texture>(new Texture(device, normalPixel, 1, 1, VK_FORMAT_R8G8B8A8_UNORM));
  }

  void Texture::createImage(int                   width,
                            int                   height,
                            uint32_t              mipLevels,
                            VkFormat              format,
                            VkImageTiling         tiling,
                            VkImageUsageFlags     usage,
                            VkMemoryPropertyFlags properties)
  {
    VkImageCreateInfo imageInfo{};
    imageInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType     = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width  = static_cast<uint32_t>(width);
    imageInfo.extent.height = static_cast<uint32_t>(height);
    imageInfo.extent.depth  = 1;
    imageInfo.mipLevels     = mipLevels;
    imageInfo.arrayLayers   = 1;
    imageInfo.format        = format;
    imageInfo.tiling        = tiling;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage         = usage;
    imageInfo.samples       = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateImage(device_.device(), &imageInfo, nullptr, &image_) != VK_SUCCESS)
    {
      throw std::runtime_error("Failed to create image!");
    }

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(device_.device(), image_, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize  = memRequirements.size;
    allocInfo.memoryTypeIndex = device_.memory().findMemoryType(memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(device_.device(), &allocInfo, nullptr, &imageMemory_) != VK_SUCCESS)
    {
      throw std::runtime_error("Failed to allocate image memory!");
    }

    vkBindImageMemory(device_.device(), image_, imageMemory_, 0);
  }

  void Texture::createImageView(VkFormat format)
  {
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image                           = image_;
    viewInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format                          = format;
    viewInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel   = 0;
    viewInfo.subresourceRange.levelCount     = mipLevels_;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount     = 1;

    if (vkCreateImageView(device_.device(), &viewInfo, nullptr, &imageView_) != VK_SUCCESS)
    {
      throw std::runtime_error("Failed to create texture image view!");
    }
  }

  void Texture::createSampler()
  {
    const VkPhysicalDeviceProperties& properties = device_.getProperties();

    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter               = VK_FILTER_LINEAR;
    samplerInfo.minFilter               = VK_FILTER_LINEAR;
    samplerInfo.addressModeU            = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV            = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW            = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.anisotropyEnable        = VK_TRUE;
    samplerInfo.maxAnisotropy           = properties.limits.maxSamplerAnisotropy;
    samplerInfo.borderColor             = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable           = VK_FALSE;
    samplerInfo.compareOp               = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode              = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.mipLodBias              = 0.0f;
    samplerInfo.minLod                  = 0.0f;
    samplerInfo.maxLod                  = static_cast<float>(mipLevels_);

    if (vkCreateSampler(device_.device(), &samplerInfo, nullptr, &sampler_) != VK_SUCCESS)
    {
      throw std::runtime_error("Failed to create texture sampler!");
    }
  }

  void Texture::transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t mipLevels)
  {
    VkCommandBuffer commandBuffer = device_.memory().beginSingleTimeCommands();

    VkImageMemoryBarrier barrier{};
    barrier.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout                       = oldLayout;
    barrier.newLayout                       = newLayout;
    barrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    barrier.image                           = image;
    barrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel   = 0;
    barrier.subresourceRange.levelCount     = mipLevels;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount     = 1;

    VkPipelineStageFlags sourceStage;
    VkPipelineStageFlags destinationStage;

    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
    {
      barrier.srcAccessMask = 0;
      barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

      sourceStage      = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
      destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
    {
      barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
      barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

      sourceStage      = VK_PIPELINE_STAGE_TRANSFER_BIT;
      destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }
    else
    {
      throw std::invalid_argument("Unsupported layout transition!");
    }

    vkCmdPipelineBarrier(commandBuffer, sourceStage, destinationStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    device_.memory().endSingleTimeCommands(commandBuffer);
  }

  void Texture::copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height)
  {
    VkCommandBuffer commandBuffer = device_.memory().beginSingleTimeCommands();

    VkBufferImageCopy region{};
    region.bufferOffset                    = 0;
    region.bufferRowLength                 = 0;
    region.bufferImageHeight               = 0;
    region.imageSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel       = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount     = 1;
    region.imageOffset                     = {0, 0, 0};
    region.imageExtent                     = {width, height, 1};

    vkCmdCopyBufferToImage(commandBuffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    device_.memory().endSingleTimeCommands(commandBuffer);
  }

  void Texture::generateMipmaps(VkImage image, VkFormat format, int32_t width, int32_t height, uint32_t mipLevels)
  {
    // Check if image format supports linear blitting
    VkFormatProperties formatProperties;
    vkGetPhysicalDeviceFormatProperties(device_.getPhysicalDevice(), format, &formatProperties);

    if (!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT))
    {
      throw std::runtime_error("Texture image format does not support linear blitting!");
    }

    VkCommandBuffer commandBuffer = device_.memory().beginSingleTimeCommands();

    VkImageMemoryBarrier barrier{};
    barrier.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.image                           = image;
    barrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    barrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount     = 1;
    barrier.subresourceRange.levelCount     = 1;

    int32_t mipWidth  = width;
    int32_t mipHeight = height;

    for (uint32_t i = 1; i < mipLevels; i++)
    {
      barrier.subresourceRange.baseMipLevel = i - 1;
      barrier.oldLayout                     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
      barrier.newLayout                     = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
      barrier.srcAccessMask                 = VK_ACCESS_TRANSFER_WRITE_BIT;
      barrier.dstAccessMask                 = VK_ACCESS_TRANSFER_READ_BIT;

      vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

      VkImageBlit blit{};
      blit.srcOffsets[0]                 = {0, 0, 0};
      blit.srcOffsets[1]                 = {mipWidth, mipHeight, 1};
      blit.srcSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
      blit.srcSubresource.mipLevel       = i - 1;
      blit.srcSubresource.baseArrayLayer = 0;
      blit.srcSubresource.layerCount     = 1;
      blit.dstOffsets[0]                 = {0, 0, 0};
      blit.dstOffsets[1]                 = {mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1};
      blit.dstSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
      blit.dstSubresource.mipLevel       = i;
      blit.dstSubresource.baseArrayLayer = 0;
      blit.dstSubresource.layerCount     = 1;

      vkCmdBlitImage(commandBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR);

      barrier.oldLayout     = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
      barrier.newLayout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
      barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
      barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

      vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

      if (mipWidth > 1) mipWidth /= 2;
      if (mipHeight > 1) mipHeight /= 2;
    }

    barrier.subresourceRange.baseMipLevel = mipLevels - 1;
    barrier.oldLayout                     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout                     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask                 = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask                 = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    device_.memory().endSingleTimeCommands(commandBuffer);
  }

  size_t Texture::getMemorySize() const
  {
    // Calculate memory for base texture + all mipmaps
    // Format: RGBA8 (4 bytes per pixel) or sRGB8_A8 (also 4 bytes)
    size_t totalSize = 0;
    int    w         = width_;
    int    h         = height_;

    for (uint32_t level = 0; level < mipLevels_; ++level)
    {
      totalSize += w * h * 4; // 4 bytes per pixel (RGBA8)
      w = std::max(1, w / 2);
      h = std::max(1, h / 2);
    }

    return totalSize;
  }

} // namespace engine
