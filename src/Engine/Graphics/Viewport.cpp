#include "Engine/Graphics/Viewport.hpp"

#include <imgui_impl_vulkan.h>

#include "Engine/Core/Exceptions.hpp"
#include "Engine/Graphics/Device.hpp"

namespace engine {

    Viewport::~Viewport() {
        destroy();
    }

    Viewport::Viewport(Viewport&& other) noexcept
        : image_(other.image_),
          memory_(other.memory_),
          imageView_(other.imageView_),
          sampler_(other.sampler_),
          format_(other.format_),
          extent_(other.extent_),
          imTextureID_(other.imTextureID_),
          device_(other.device_) {
        other.image_       = VK_NULL_HANDLE;
        other.memory_      = VK_NULL_HANDLE;
        other.imageView_   = VK_NULL_HANDLE;
        other.sampler_     = VK_NULL_HANDLE;
        other.format_      = VK_FORMAT_UNDEFINED;
        other.extent_      = {0, 0};
        other.imTextureID_ = (ImTextureID) nullptr;
        other.device_      = nullptr;
    }

    Viewport& Viewport::operator=(Viewport&& other) noexcept {
        if (this != &other) {
            destroy();
            image_       = other.image_;
            memory_      = other.memory_;
            imageView_   = other.imageView_;
            sampler_     = other.sampler_;
            format_      = other.format_;
            extent_      = other.extent_;
            imTextureID_ = other.imTextureID_;
            device_      = other.device_;

            other.image_       = VK_NULL_HANDLE;
            other.memory_      = VK_NULL_HANDLE;
            other.imageView_   = VK_NULL_HANDLE;
            other.sampler_     = VK_NULL_HANDLE;
            other.format_      = VK_FORMAT_UNDEFINED;
            other.extent_      = {0, 0};
            other.imTextureID_ = (ImTextureID) nullptr;
            other.device_      = nullptr;
        }
        return *this;
    }

    void Viewport::create(Device& device, VkExtent2D extent) {
        device_ = &device;
        extent_ = extent;
        createImage(device, extent);
        createImageView(device);
        createSampler(device);
        registerWithImGui(device);
    }

    void Viewport::resize(Device& device, VkExtent2D newExtent) {
        // ImGui texture handle is auto-managed; just recreate the Vulkan resources.
        destroy();
        create(device, newExtent);
    }

    void Viewport::destroy() {
        if (!device_)
            return;

        // ImGui handles its own texture lifecycle — no manual removal needed
        // since ImGui_ImplVulkan_AddTexture creates descriptors that live in
        // ImGui's descriptor pool and are cleaned up on ImGui shutdown.

        if (sampler_ != VK_NULL_HANDLE) {
            vkDestroySampler(device_->device(), sampler_, nullptr);
            sampler_ = VK_NULL_HANDLE;
        }
        if (imageView_ != VK_NULL_HANDLE) {
            vkDestroyImageView(device_->device(), imageView_, nullptr);
            imageView_ = VK_NULL_HANDLE;
        }
        if (image_ != VK_NULL_HANDLE) {
            vkDestroyImage(device_->device(), image_, nullptr);
            image_ = VK_NULL_HANDLE;
        }
        if (memory_ != VK_NULL_HANDLE) {
            vkFreeMemory(device_->device(), memory_, nullptr);
            memory_ = VK_NULL_HANDLE;
        }

        imTextureID_ = (ImTextureID) nullptr;
        device_      = nullptr;
    }

    void Viewport::transitionToTransferDst(VkCommandBuffer cmd, VkImageLayout sourceLayout) const {
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

        vkCmdPipelineBarrier(cmd,
            srcStage,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_DEPENDENCY_BY_REGION_BIT,
            0, nullptr,
            0, nullptr,
            1, &barrier);
    }

    void Viewport::transitionToShaderReadOnly(VkCommandBuffer cmd, VkImageLayout sourceLayout) const {
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

        vkCmdPipelineBarrier(cmd,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            VK_DEPENDENCY_BY_REGION_BIT,
            0, nullptr,
            0, nullptr,
            1, &barrier);
    }

    VkDescriptorImageInfo Viewport::getDescriptorImageInfo() const {
        VkDescriptorImageInfo info{};
        info.sampler     = sampler_;
        info.imageView   = imageView_;
        info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        return info;
    }

    void Viewport::createImage(Device& device, VkExtent2D extent) {
        VkImageCreateInfo imageInfo{};
        imageInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType     = VK_IMAGE_TYPE_2D;
        imageInfo.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
        imageInfo.extent.width  = extent.width;
        imageInfo.extent.height = extent.height;
        imageInfo.extent.depth  = 1;
        imageInfo.mipLevels     = 1;
        imageInfo.arrayLayers   = 1;
        imageInfo.format        = VK_FORMAT_R16G16B16A16_SFLOAT;
        imageInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage         = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        imageInfo.samples       = VK_SAMPLE_COUNT_1_BIT;

        device.getMemory().createImageWithInfo(imageInfo, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, image_, memory_);
        format_ = VK_FORMAT_R16G16B16A16_SFLOAT;
    }

    void Viewport::createImageView(Device& device) {
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
            throw RuntimeException("failed to create viewport image view!");
        }
    }

    void Viewport::createSampler(Device& device) {
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
        samplerInfo.minLod                  = 0.0f;
        samplerInfo.maxLod                  = 0.0f;

        if (vkCreateSampler(device.device(), &samplerInfo, nullptr, &sampler_) != VK_SUCCESS) {
            throw RuntimeException("failed to create viewport sampler!");
        }
    }

    void Viewport::registerWithImGui(Device& device) {
        imTextureID_ = reinterpret_cast<ImTextureID>(
            ImGui_ImplVulkan_AddTexture(sampler_, imageView_, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL));
    }

}  // namespace engine
