#pragma once

#include <vulkan/vulkan.h>

#include <memory>
#include <string>

#include "Engine/Graphics/Device.hpp"

namespace engine {

  class Texture
  {
  public:
    Texture(Device& device, const std::string& filepath, bool srgb = true);
    ~Texture();

    Texture(const Texture&)            = delete;
    Texture& operator=(const Texture&) = delete;
    Texture(Texture&&)                 = delete;
    Texture& operator=(Texture&&)      = delete;

    // Create simple single-color textures (1x1 pixel)
    static std::shared_ptr<Texture> createWhiteTexture(Device& device);
    static std::shared_ptr<Texture> createNormalTexture(Device& device); // Flat normal (0.5, 0.5, 1.0)

    VkImageView           getImageView() const { return imageView_; }
    VkSampler             getSampler() const { return sampler_; }
    VkImage               getImage() const { return image_; }
    VkDescriptorImageInfo getDescriptorInfo() const
    {
      return VkDescriptorImageInfo{
              .sampler     = sampler_,
              .imageView   = imageView_,
              .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
      };
    }

    int getWidth() const { return width_; }
    int getHeight() const { return height_; }
    int getMipLevels() const { return mipLevels_; }

    void     setGlobalIndex(uint32_t index) { globalIndex_ = index; }
    uint32_t getGlobalIndex() const { return globalIndex_; }

    /**
     * @brief Get approximate memory size of this texture
     * @return Memory size in bytes (includes mipmaps)
     */
    size_t getMemorySize() const;

  private:
    // Private constructor for creating textures from memory
    Texture(Device& device, const unsigned char* pixels, int width, int height, VkFormat format);

    void
    createImage(int width, int height, uint32_t mipLevels, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties);
    void createImageView(VkFormat format);
    void createSampler();
    void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t mipLevels);
    void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);
    void generateMipmaps(VkImage image, VkFormat format, int32_t width, int32_t height, uint32_t mipLevels);

    Device& device_;

    VkImage        image_       = VK_NULL_HANDLE;
    VkDeviceMemory imageMemory_ = VK_NULL_HANDLE;
    VkImageView    imageView_   = VK_NULL_HANDLE;
    VkSampler      sampler_     = VK_NULL_HANDLE;

    int      width_       = 0;
    int      height_      = 0;
    uint32_t mipLevels_   = 1;
    uint32_t globalIndex_ = 0;
  };

} // namespace engine
