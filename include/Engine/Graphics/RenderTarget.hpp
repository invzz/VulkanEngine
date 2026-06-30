#ifndef VULKANENGINE_INCLUDE_ENGINE_GRAPHICS_RENDERTARGET_HPP
#define VULKANENGINE_INCLUDE_ENGINE_GRAPHICS_RENDERTARGET_HPP

#include <vector>

#include "Engine/Graphics/Device.hpp"

namespace engine {

    class RenderTarget {
       public:
        struct CreateInfo {
            VkExtent2D            extent{};
            VkFormat              format{VK_FORMAT_UNDEFINED};
            uint32_t              mipLevels{1};
            VkImageUsageFlags     usage{0};
            VkImageAspectFlags    aspectMask{VK_IMAGE_ASPECT_COLOR_BIT};
            VkImageTiling         tiling{VK_IMAGE_TILING_OPTIMAL};
            VkSampleCountFlagBits samples{VK_SAMPLE_COUNT_1_BIT};
            VkMemoryPropertyFlags memoryProperties{VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT};
            bool                  createPerMipViews{false};
            bool                  createAttachmentView{false};
            uint32_t              attachmentBaseMipLevel{0};
            uint32_t              attachmentLevelCount{1};
            bool                  createSampler{false};
            bool                  useSamplerCache{true};
            VkSamplerCreateInfo   samplerInfo{};
        };

        RenderTarget()  = default;
        ~RenderTarget() = default;

        RenderTarget(const RenderTarget&)            = delete;
        RenderTarget& operator=(const RenderTarget&) = delete;

        RenderTarget(RenderTarget&& other) noexcept;
        RenderTarget& operator=(RenderTarget&& other) noexcept;

        void create(Device& device, const CreateInfo& info);
        void destroy(Device& device);

        [[nodiscard]] VkImage getImage() const {
            return image_;
        }
        [[nodiscard]] VkDeviceMemory getMemory() const {
            return memory_;
        }
        [[nodiscard]] VkImageView getView() const {
            return view_;
        }
        [[nodiscard]] VkImageView getAttachmentView() const {
            return attachmentView_ != VK_NULL_HANDLE ? attachmentView_ : view_;
        }
        [[nodiscard]] VkSampler getSampler() const {
            return sampler_;
        }
        [[nodiscard]] VkFormat getFormat() const {
            return format_;
        }
        [[nodiscard]] uint32_t getMipLevels() const {
            return mipLevels_;
        }

        [[nodiscard]] VkImageView                     getMipView(uint32_t mipLevel) const;
        [[nodiscard]] const std::vector<VkImageView>& getMipViews() const {
            return mipViews_;
        }

       private:
        void reset();

        VkImage                  image_{VK_NULL_HANDLE};
        VkDeviceMemory           memory_{VK_NULL_HANDLE};
        VkImageView              view_{VK_NULL_HANDLE};
        VkImageView              attachmentView_{VK_NULL_HANDLE};
        VkSampler                sampler_{VK_NULL_HANDLE};
        bool                     samplerOwnedByCache_{false};
        std::vector<VkImageView> mipViews_;

        VkFormat           format_{VK_FORMAT_UNDEFINED};
        VkImageAspectFlags aspectMask_{VK_IMAGE_ASPECT_COLOR_BIT};
        uint32_t           mipLevels_{1};
    };

}  // namespace engine

#endif  // VULKANENGINE_INCLUDE_ENGINE_GRAPHICS_RENDERTARGET_HPP
