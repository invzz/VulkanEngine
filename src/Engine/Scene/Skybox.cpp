#include "Engine/Scene/Skybox.hpp"

#include <cstring>
#include <stdexcept>

#define STB_IMAGE_IMPLEMENTATION_ALREADY_DEFINED
#include <stb_image.h>

#include "Engine/Graphics/Buffer.hpp"

namespace engine {

  Skybox::Skybox(Device& device, const std::array<std::string, 6>& facePaths) : device_(device)
  {
    createCubemapImage(facePaths);
    createImageView();
    createSampler();
  }

  Skybox::Skybox(Device& device, uint32_t size) : device_(device), size_(static_cast<int>(size))
  {
    // Create cubemap image for rendering
    VkImageCreateInfo imageInfo{};
    imageInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType     = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width  = size;
    imageInfo.extent.height = size;
    imageInfo.extent.depth  = 1;
    imageInfo.mipLevels     = 1;
    imageInfo.arrayLayers   = 6;                        // 6 faces
    imageInfo.format        = VK_FORMAT_R8G8B8A8_UNORM; // UNORM to store gamma-corrected values from shader
    imageInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage         = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    imageInfo.samples       = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.flags         = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;

    if (vkCreateImage(device_.device(), &imageInfo, nullptr, &image_) != VK_SUCCESS)
    {
      throw std::runtime_error("Failed to create procedural skybox image");
    }

    // Allocate memory
    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(device_.device(), image_, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize  = memRequirements.size;
    allocInfo.memoryTypeIndex = device_.memory().findMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (vkAllocateMemory(device_.device(), &allocInfo, nullptr, &imageMemory_) != VK_SUCCESS)
    {
      throw std::runtime_error("Failed to allocate procedural skybox image memory");
    }

    vkBindImageMemory(device_.device(), image_, imageMemory_, 0);

    createImageView();
    createSampler();
  }

  Skybox::~Skybox()
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

  std::unique_ptr<Skybox> Skybox::loadFromFolder(Device& device, const std::string& folderPath, const std::string& extension)
  {
    std::array<std::string, 6> facePaths = {
            folderPath + "/posx." + extension, // +X (right)
            folderPath + "/negx." + extension, // -X (left)
            folderPath + "/posy." + extension, // +Y (top)
            folderPath + "/negy." + extension, // -Y (bottom)
            folderPath + "/posz." + extension, // +Z (front)
            folderPath + "/negz." + extension, // -Z (back)
    };

    return std::make_unique<Skybox>(device, facePaths);
  }

  void Skybox::createCubemapImage(const std::array<std::string, 6>& facePaths)
  {
    // Load all 6 faces and determine size
    std::array<unsigned char*, 6> faceData{};
    int                           width = 0, height = 0, channels = 0;

    for (int i = 0; i < 6; i++)
    {
      faceData[i] = stbi_load(facePaths[i].c_str(), &width, &height, &channels, STBI_rgb_alpha);
      if (!faceData[i])
      {
        // Clean up already loaded faces
        for (int j = 0; j < i; j++)
        {
          stbi_image_free(faceData[j]);
        }
        throw std::runtime_error("Failed to load skybox face: " + facePaths[i]);
      }

      // Verify all faces are same size
      if (i == 0)
      {
        size_ = width;
        if (width != height)
        {
          stbi_image_free(faceData[i]);
          throw std::runtime_error("Skybox faces must be square: " + facePaths[i]);
        }
      }
      else if (width != size_ || height != size_)
      {
        for (int j = 0; j <= i; j++)
        {
          stbi_image_free(faceData[j]);
        }
        throw std::runtime_error("All skybox faces must be same size: " + facePaths[i]);
      }
    }

    VkDeviceSize faceSize  = static_cast<VkDeviceSize>(size_) * size_ * 4; // RGBA
    VkDeviceSize totalSize = faceSize * 6;

    // Create staging buffer with all face data
    Buffer stagingBuffer{
            device_,
            faceSize,
            6,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
    };

    stagingBuffer.map();
    for (int i = 0; i < 6; i++)
    {
      stagingBuffer.writeToIndex(faceData[i], i);
      stbi_image_free(faceData[i]);
    }
    stagingBuffer.unmap();

    // Create cubemap image
    VkImageCreateInfo imageInfo{};
    imageInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType     = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width  = static_cast<uint32_t>(size_);
    imageInfo.extent.height = static_cast<uint32_t>(size_);
    imageInfo.extent.depth  = 1;
    imageInfo.mipLevels     = 1;
    imageInfo.arrayLayers   = 6; // 6 faces
    imageInfo.format        = VK_FORMAT_R8G8B8A8_SRGB;
    imageInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage         = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.samples       = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.flags         = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;

    if (vkCreateImage(device_.device(), &imageInfo, nullptr, &image_) != VK_SUCCESS)
    {
      throw std::runtime_error("Failed to create cubemap image");
    }

    // Allocate memory
    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(device_.device(), image_, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize  = memRequirements.size;
    allocInfo.memoryTypeIndex = device_.memory().findMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (vkAllocateMemory(device_.device(), &allocInfo, nullptr, &imageMemory_) != VK_SUCCESS)
    {
      throw std::runtime_error("Failed to allocate cubemap image memory");
    }

    vkBindImageMemory(device_.device(), image_, imageMemory_, 0);

    // Transition to transfer destination
    transitionImageLayout(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    // Copy each face from staging buffer
    VkCommandBuffer commandBuffer = device_.memory().beginSingleTimeCommands();

    std::array<VkBufferImageCopy, 6> regions{};
    for (int i = 0; i < 6; i++)
    {
      regions[i].bufferOffset                    = i * faceSize;
      regions[i].bufferRowLength                 = 0;
      regions[i].bufferImageHeight               = 0;
      regions[i].imageSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
      regions[i].imageSubresource.mipLevel       = 0;
      regions[i].imageSubresource.baseArrayLayer = i;
      regions[i].imageSubresource.layerCount     = 1;
      regions[i].imageOffset                     = {0, 0, 0};
      regions[i].imageExtent                     = {static_cast<uint32_t>(size_), static_cast<uint32_t>(size_), 1};
    }

    vkCmdCopyBufferToImage(commandBuffer, stagingBuffer.getBuffer(), image_, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 6, regions.data());

    device_.memory().endSingleTimeCommands(commandBuffer);

    // Transition to shader read
    transitionImageLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  }

  void Skybox::createImageView()
  {
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image                           = image_;
    viewInfo.viewType                        = VK_IMAGE_VIEW_TYPE_CUBE;
    viewInfo.format                          = VK_FORMAT_R8G8B8A8_SRGB;
    viewInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel   = 0;
    viewInfo.subresourceRange.levelCount     = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount     = 6;

    if (vkCreateImageView(device_.device(), &viewInfo, nullptr, &imageView_) != VK_SUCCESS)
    {
      throw std::runtime_error("Failed to create cubemap image view");
    }
  }

  void Skybox::createSampler()
  {
    const VkPhysicalDeviceProperties& properties = device_.getProperties();

    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter               = VK_FILTER_LINEAR;
    samplerInfo.minFilter               = VK_FILTER_LINEAR;
    samplerInfo.addressModeU            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.anisotropyEnable        = VK_TRUE;
    samplerInfo.maxAnisotropy           = properties.limits.maxSamplerAnisotropy;
    samplerInfo.borderColor             = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable           = VK_FALSE;
    samplerInfo.compareOp               = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode              = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.mipLodBias              = 0.0f;
    samplerInfo.minLod                  = 0.0f;
    samplerInfo.maxLod                  = 0.0f;

    if (vkCreateSampler(device_.device(), &samplerInfo, nullptr, &sampler_) != VK_SUCCESS)
    {
      throw std::runtime_error("Failed to create cubemap sampler");
    }
  }

  void Skybox::transitionImageLayout(VkImageLayout oldLayout, VkImageLayout newLayout)
  {
    VkCommandBuffer commandBuffer = device_.memory().beginSingleTimeCommands();

    VkImageMemoryBarrier barrier{};
    barrier.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout                       = oldLayout;
    barrier.newLayout                       = newLayout;
    barrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    barrier.image                           = image_;
    barrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel   = 0;
    barrier.subresourceRange.levelCount     = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount     = 6;

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
      throw std::runtime_error("Unsupported layout transition");
    }

    vkCmdPipelineBarrier(commandBuffer, sourceStage, destinationStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    device_.memory().endSingleTimeCommands(commandBuffer);
  }

} // namespace engine
