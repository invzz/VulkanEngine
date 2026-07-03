#include "Engine/Graphics/RenderTarget.hpp"

#include <stdexcept>

namespace engine {

    RenderTarget::RenderTarget(RenderTarget&& other) noexcept {
        *this = std::move(other);
    }

    RenderTarget& RenderTarget::operator=(RenderTarget&& other) noexcept {
        if (this == &other) {
            return *this;
        }

        image_               = other.image_;
        memory_              = other.memory_;
        view_                = other.view_;
        attachmentView_      = other.attachmentView_;
        sampler_             = other.sampler_;
        samplerOwnedByCache_ = other.samplerOwnedByCache_;
        mipViews_            = std::move(other.mipViews_);
        format_              = other.format_;
        aspectMask_          = other.aspectMask_;
        mipLevels_           = other.mipLevels_;

        other.reset();
        return *this;
    }

    void RenderTarget::create(Device& device, const CreateInfo& info) {
        if (info.format == VK_FORMAT_UNDEFINED) {
            throw std::runtime_error("RenderTarget::create called with undefined format");
        }

        VkImageCreateInfo imageInfo{};
        imageInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType     = VK_IMAGE_TYPE_2D;
        imageInfo.extent.width  = info.extent.width;
        imageInfo.extent.height = info.extent.height;
        imageInfo.extent.depth  = 1;
        imageInfo.mipLevels     = info.mipLevels;
        imageInfo.arrayLayers   = 1;
        imageInfo.format        = info.format;
        imageInfo.tiling        = info.tiling;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage         = info.usage;
        imageInfo.samples       = info.samples;
        imageInfo.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;

        device.getMemory().createImageWithInfo(imageInfo, info.memoryProperties, image_, memory_);

        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image                           = image_;
        viewInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format                          = info.format;
        viewInfo.subresourceRange.aspectMask     = info.aspectMask;
        viewInfo.subresourceRange.baseMipLevel   = 0;
        viewInfo.subresourceRange.levelCount     = info.mipLevels;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount     = 1;

        if (vkCreateImageView(device.device(), &viewInfo, nullptr, &view_) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create render target image view");
        }

        if (info.createAttachmentView || info.mipLevels > 1) {
            VkImageViewCreateInfo attachmentViewInfo           = viewInfo;
            attachmentViewInfo.subresourceRange.baseMipLevel   = info.createAttachmentView ? info.attachmentBaseMipLevel : 0;
            attachmentViewInfo.subresourceRange.levelCount     = info.createAttachmentView ? info.attachmentLevelCount : 1;
            attachmentViewInfo.subresourceRange.baseArrayLayer = 0;
            attachmentViewInfo.subresourceRange.layerCount     = 1;
            if (vkCreateImageView(device.device(), &attachmentViewInfo, nullptr, &attachmentView_) != VK_SUCCESS) {
                throw std::runtime_error("Failed to create render target attachment view");
            }
        }

        if (info.createPerMipViews) {
            mipViews_.resize(info.mipLevels);
            for (uint32_t mip = 0; mip < info.mipLevels; ++mip) {
                VkImageViewCreateInfo mipViewInfo         = viewInfo;
                mipViewInfo.subresourceRange.baseMipLevel = mip;
                mipViewInfo.subresourceRange.levelCount   = 1;
                if (vkCreateImageView(device.device(), &mipViewInfo, nullptr, &mipViews_[mip]) != VK_SUCCESS) {
                    throw std::runtime_error("Failed to create render target mip image view");
                }
            }
        }

        if (info.createSampler) {
            if (info.samplerInfo.sType != VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO) {
                throw std::runtime_error("RenderTarget::create sampler requested with invalid sampler create info");
            }

            if (info.useSamplerCache) {
                sampler_             = device.getOrCreateSampler(info.samplerInfo);
                samplerOwnedByCache_ = true;
            } else {
                if (vkCreateSampler(device.device(), &info.samplerInfo, nullptr, &sampler_) != VK_SUCCESS) {
                    throw std::runtime_error("Failed to create render target sampler");
                }
                samplerOwnedByCache_ = false;
            }

            if (sampler_ == VK_NULL_HANDLE) {
                throw std::runtime_error("RenderTarget::create failed to acquire sampler");
            }
        }

        format_     = info.format;
        aspectMask_ = info.aspectMask;
        mipLevels_  = info.mipLevels;
    }

    void RenderTarget::destroy(Device& device) {
        for (auto mipView : mipViews_) {
            if (mipView != VK_NULL_HANDLE) {
                vkDestroyImageView(device.device(), mipView, nullptr);
            }
        }
        mipViews_.clear();

        if (attachmentView_ != VK_NULL_HANDLE) {
            vkDestroyImageView(device.device(), attachmentView_, nullptr);
            attachmentView_ = VK_NULL_HANDLE;
        }

        if (view_ != VK_NULL_HANDLE) {
            vkDestroyImageView(device.device(), view_, nullptr);
            view_ = VK_NULL_HANDLE;
        }

        if (sampler_ != VK_NULL_HANDLE && !samplerOwnedByCache_) {
            vkDestroySampler(device.device(), sampler_, nullptr);
        }
        sampler_             = VK_NULL_HANDLE;
        samplerOwnedByCache_ = false;

        if (image_ != VK_NULL_HANDLE) {
            vkDestroyImage(device.device(), image_, nullptr);
            image_ = VK_NULL_HANDLE;
        }

        if (memory_ != VK_NULL_HANDLE) {
            vkFreeMemory(device.device(), memory_, nullptr);
            memory_ = VK_NULL_HANDLE;
        }
    }

    VkImageView RenderTarget::getMipView(uint32_t mipLevel) const {
        if (mipLevel >= mipViews_.size()) {
            return VK_NULL_HANDLE;
        }
        return mipViews_[mipLevel];
    }

    void RenderTarget::reset() {
        image_               = VK_NULL_HANDLE;
        memory_              = VK_NULL_HANDLE;
        view_                = VK_NULL_HANDLE;
        attachmentView_      = VK_NULL_HANDLE;
        sampler_             = VK_NULL_HANDLE;
        samplerOwnedByCache_ = false;
        mipViews_.clear();
        format_     = VK_FORMAT_UNDEFINED;
        aspectMask_ = VK_IMAGE_ASPECT_COLOR_BIT;
        mipLevels_  = 1;
    }

}  // namespace engine
