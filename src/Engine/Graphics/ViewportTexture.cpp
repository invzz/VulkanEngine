#include "Engine/Graphics/ViewportTexture.hpp"

#include <vulkan/vulkan_core.h>

#include "Engine/Core/Exceptions.hpp"
#include "Engine/Core/Logger.hpp"
#include "Engine/Graphics/Device.hpp"

namespace engine {

    ViewportTexture::~ViewportTexture() = default;

    ViewportTexture::ViewportTexture(ViewportTexture&& other) noexcept
        : image_(other.image_),
          memory_(other.memory_),
          imageView_(other.imageView_),
          sampler_(other.sampler_),
          format_(other.format_),
          extent_(other.extent_),
          mipLevels_(other.mipLevels_),
          samplerOwnedByCache_(other.samplerOwnedByCache_) {
        other.image_               = VK_NULL_HANDLE;
        other.memory_              = VK_NULL_HANDLE;
        other.imageView_           = VK_NULL_HANDLE;
        other.sampler_             = VK_NULL_HANDLE;
        other.format_              = VK_FORMAT_UNDEFINED;
        other.extent_              = {0, 0};
        other.mipLevels_           = 1;
        other.samplerOwnedByCache_ = false;
    }

    ViewportTexture& ViewportTexture::operator=(ViewportTexture&& other) noexcept {
        if (this != &other) {
            image_               = other.image_;
            memory_              = other.memory_;
            imageView_           = other.imageView_;
            sampler_             = other.sampler_;
            format_              = other.format_;
            extent_              = other.extent_;
            mipLevels_           = other.mipLevels_;
            samplerOwnedByCache_ = other.samplerOwnedByCache_;

            other.image_               = VK_NULL_HANDLE;
            other.memory_              = VK_NULL_HANDLE;
            other.imageView_           = VK_NULL_HANDLE;
            other.sampler_             = VK_NULL_HANDLE;
            other.format_              = VK_FORMAT_UNDEFINED;
            other.extent_              = {0, 0};
            other.mipLevels_           = 1;
            other.samplerOwnedByCache_ = false;
        }
        return *this;
    }

    void ViewportTexture::create(Device& device, VkExtent2D extent) {
        extent_ = extent;
        createImage(device, extent);
        createImageView(device);
        createSampler(device);
    }

    void ViewportTexture::destroy(Device& device) {
        if (sampler_ != VK_NULL_HANDLE) {
            vkDestroySampler(device.device(), sampler_, nullptr);
            sampler_ = VK_NULL_HANDLE;
        }
        if (imageView_ != VK_NULL_HANDLE) {
            vkDestroyImageView(device.device(), imageView_, nullptr);
            imageView_ = VK_NULL_HANDLE;
        }
        if (image_ != VK_NULL_HANDLE) {
            vkDestroyImage(device.device(), image_, nullptr);
            image_ = VK_NULL_HANDLE;
        }
        if (memory_ != VK_NULL_HANDLE) {
            vkFreeMemory(device.device(), memory_, nullptr);
            memory_ = VK_NULL_HANDLE;
        }
    }

    void ViewportTexture::resize(Device& device, VkExtent2D newExtent) {
        destroy(device);
        extent_ = newExtent;
        createImage(device, newExtent);
        createImageView(device);
        createSampler(device);
    }

    void ViewportTexture::transitionToColorAttachment(VkCommandBuffer commandBuffer) const {
        VkImageMemoryBarrier barrier{};
        barrier.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout                       = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.newLayout                       = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        barrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
        barrier.image                           = image_;
        barrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel   = 0;
        barrier.subresourceRange.levelCount     = VK_REMAINING_MIP_LEVELS;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount     = 1;
        barrier.srcAccessMask                   = VK_ACCESS_SHADER_READ_BIT;
        barrier.dstAccessMask                   = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        vkCmdPipelineBarrier(commandBuffer,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_DEPENDENCY_BY_REGION_BIT,
            0,
            nullptr,
            0,
            nullptr,
            1,
            &barrier);
    }

    void ViewportTexture::transitionToTransferDst(VkCommandBuffer commandBuffer, VkImageLayout sourceLayout) const {
        VkImageMemoryBarrier barrier{};
        barrier.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout                       = sourceLayout;
        barrier.newLayout                       = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
        barrier.image                           = image_;
        barrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel   = 0;
        barrier.subresourceRange.levelCount     = VK_REMAINING_MIP_LEVELS;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount     = 1;
        barrier.srcAccessMask                   = 0;
        barrier.dstAccessMask                   = VK_ACCESS_TRANSFER_WRITE_BIT;

        VkPipelineStageFlags srcStage = (sourceLayout == VK_IMAGE_LAYOUT_UNDEFINED)
            ? VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT
            : VK_PIPELINE_STAGE_HOST_BIT;

        vkCmdPipelineBarrier(commandBuffer,
            srcStage,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_DEPENDENCY_BY_REGION_BIT,
            0,
            nullptr,
            0,
            nullptr,
            1,
            &barrier);
    }

    void ViewportTexture::transitionToShaderReadOnly(VkCommandBuffer commandBuffer, VkImageLayout sourceLayout) const {
        VkImageMemoryBarrier barrier{};
        barrier.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout                       = sourceLayout;
        barrier.newLayout                       = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
        barrier.image                           = image_;
        barrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel   = 0;
        barrier.subresourceRange.levelCount     = VK_REMAINING_MIP_LEVELS;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount     = 1;
        barrier.srcAccessMask                   = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask                   = VK_ACCESS_SHADER_READ_BIT;

        vkCmdPipelineBarrier(commandBuffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            VK_DEPENDENCY_BY_REGION_BIT,
            0,
            nullptr,
            0,
            nullptr,
            1,
            &barrier);
    }

    void ViewportTexture::createImage(Device& device, VkExtent2D extent) {
        VkImageCreateInfo imageInfo{};
        imageInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType     = VK_IMAGE_TYPE_2D;
        imageInfo.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
        imageInfo.extent.width  = extent.width;
        imageInfo.extent.height = extent.height;
        imageInfo.extent.depth  = 1;
        imageInfo.mipLevels     = mipLevels_;
        imageInfo.arrayLayers   = 1;
        imageInfo.format        = VK_FORMAT_R16G16B16A16_SFLOAT;
        imageInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage         = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        imageInfo.samples       = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.flags         = 0;

        if (vkCreateImage(device.device(), &imageInfo, nullptr, &image_) != VK_SUCCESS) {
            throw engine::RuntimeException("failed to create viewport texture image!");
        }

        VkMemoryRequirements memRequirements;
        vkGetImageMemoryRequirements(device.device(), image_, &memRequirements);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize  = memRequirements.size;
        allocInfo.memoryTypeIndex = device.getMemory().findMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        if (vkAllocateMemory(device.device(), &allocInfo, nullptr, &memory_) != VK_SUCCESS) {
            throw engine::RuntimeException("failed to allocate viewport texture memory!");
        }

        if (vkBindImageMemory(device.device(), image_, memory_, 0) != VK_SUCCESS) {
            throw engine::RuntimeException("failed to bind viewport texture memory!");
        }

        format_ = VK_FORMAT_R16G16B16A16_SFLOAT;
    }

    void ViewportTexture::createImageView(Device& device) {
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image                           = image_;
        viewInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format                          = format_;
        viewInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel   = 0;
        viewInfo.subresourceRange.levelCount     = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount     = 1;

        if (vkCreateImageView(device.device(), &viewInfo, nullptr, &imageView_) != VK_SUCCESS) {
            throw engine::RuntimeException("failed to create viewport texture image view!");
        }
    }

    void ViewportTexture::createSampler(Device& device) {
        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter               = VK_FILTER_LINEAR;
        samplerInfo.minFilter               = VK_FILTER_LINEAR;
        samplerInfo.addressModeU            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeV            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeW            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.anisotropyEnable        = VK_FALSE;
        samplerInfo.maxAnisotropy           = 1.0f;
        samplerInfo.borderColor             = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        samplerInfo.unnormalizedCoordinates = VK_FALSE;
        samplerInfo.compareEnable           = VK_FALSE;
        samplerInfo.compareOp               = VK_COMPARE_OP_ALWAYS;
        samplerInfo.mipmapMode              = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerInfo.mipLodBias              = 0.0f;
        samplerInfo.minLod                  = 0.0f;
        samplerInfo.maxLod                  = 0.0f;

        if (vkCreateSampler(device.device(), &samplerInfo, nullptr, &sampler_) != VK_SUCCESS) {
            throw engine::RuntimeException("failed to create viewport texture sampler!");
        }
    }

}  // namespace engine
