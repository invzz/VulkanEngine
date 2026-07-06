#ifndef VULKANENGINE_INCLUDE_ENGINE_RESOURCES_TEXTURE_HPP
#define VULKANENGINE_INCLUDE_ENGINE_RESOURCES_TEXTURE_HPP
#include <vulkan/vulkan.h>

#include <memory>
#include <string>

#include "Engine/Graphics/Device.hpp"
namespace engine {
    class Texture {
       public:
        Texture(Device& device, const std::string& filepath, bool srgb = true, bool flipY = false);
        ~Texture();
        Texture(const Texture&)                              = delete;
        Texture& operator=(const Texture&)                   = delete;
        Texture(Texture&&)                                   = delete;
        Texture&                        operator=(Texture&&) = delete;
        static std::shared_ptr<Texture> createWhiteTexture(Device& device);
        static std::shared_ptr<Texture> createNormalTexture(Device& device);
        static std::shared_ptr<Texture> createFromEXR(Device& device, const std::string& filepath);
        static std::shared_ptr<Texture>
                                        createFromEXR_CPUOnly(Device& device, const std::string& exrPath, const std::string& outVtexPath, bool loadIntoGpu = false, VkFormat targetFormat = VK_FORMAT_R32G32B32A32_SFLOAT);
        static std::shared_ptr<Texture> createFromVTEX(Device& device, const std::string& filepath);
        static std::shared_ptr<Texture> createFromDecoded(Device& device, const unsigned char* pixels, int width, int height, VkFormat format);

       public:
        [[nodiscard]] VkImageView getImageView() const {
            return imageView_;
        }
        [[nodiscard]] VkSampler getSampler() const {
            return sampler_;
        }
        [[nodiscard]] VkImage getImage() const {
            return image_;
        }
        [[nodiscard]] VkDescriptorImageInfo getDescriptorInfo() const {
            return VkDescriptorImageInfo{
                .sampler     = sampler_,
                .imageView   = imageView_,
                .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            };
        }
        [[nodiscard]] int getWidth() const {
            return width_;
        }
        [[nodiscard]] int getHeight() const {
            return height_;
        }
        [[nodiscard]] int getMipLevels() const {
            return mipLevels_;
        }
        void setGlobalIndex(uint32_t index) {
            globalIndex_ = index;
        }
        [[nodiscard]] uint32_t getGlobalIndex() const {
            return globalIndex_;
        }
        static std::shared_ptr<Texture> createFromVTEX(Device& device, const std::string& filepath, bool cpuOnly);
        /**
   * @brief Get approximate memory size of this texture
   * @return Memory size in bytes (includes mipmaps)
   */
        [[nodiscard]] size_t getMemorySize() const;

       private:
        Texture(Device& device, VkImage image, VkDeviceMemory memory, VkImageView view, VkSampler sampler, int width, int height, uint32_t mipLevels, VkFormat format);
        Texture(Device& device, const unsigned char* pixels, int width, int height, VkFormat format);
        Texture(Device& device, const float* pixels, int width, int height, VkFormat format);
        Texture(Device& device, int width, int height, uint32_t mipLevels, VkFormat format, bool cpuOnly);
        void           createImage(int width, int height, uint32_t mipLevels, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties);
        void           createImageView(VkFormat format);
        void           createSampler();
        void           transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t mipLevels);
        void           copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);
        void           generateMipmaps(VkImage image, VkFormat format, int32_t width, int32_t height, uint32_t mipLevels);
        Device&        device_;
        VkImage        image_               = VK_NULL_HANDLE;
        VkDeviceMemory imageMemory_         = VK_NULL_HANDLE;
        VkImageView    imageView_           = VK_NULL_HANDLE;
        VkSampler      sampler_             = VK_NULL_HANDLE;
        int            width_               = 0;
        int            height_              = 0;
        uint32_t       mipLevels_           = 1;
        uint32_t       globalIndex_         = 0;
        VkFormat       format_              = VK_FORMAT_UNDEFINED;
        bool           cpuOnly_             = false;
        bool           samplerOwnedByCache_ = false;

       public:
        [[nodiscard]] VkFormat getFormat() const {
            return format_;
        }
    };
}  // namespace engine
#endif
