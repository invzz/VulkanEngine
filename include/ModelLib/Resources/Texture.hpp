#ifndef VULKANENGINE_INCLUDE_ENGINE_RESOURCES_TEXTURE_HPP
#define VULKANENGINE_INCLUDE_ENGINE_RESOURCES_TEXTURE_HPP

#include <vulkan/vulkan.h>

#include <memory>
#include <string>

#include "Engine/Graphics/Device.hpp"

namespace engine {

  class Texture
  {
  public:
    Texture(Device& device, const std::string& filepath, bool srgb = true, bool flipY = false);
    ~Texture();

    Texture(const Texture&)            = delete;
    Texture& operator=(const Texture&) = delete;
    Texture(Texture&&)                 = delete;
    Texture& operator=(Texture&&)      = delete;

    // Create simple single-color textures (1x1 pixel)
    static std::shared_ptr<Texture> createWhiteTexture(Device& device);
    static std::shared_ptr<Texture> createNormalTexture(Device& device); // Flat normal (0.5, 0.5, 1.0)

    // Load high-dynamic-range EXR textures as linear float RGBA
    static std::shared_ptr<Texture> createFromEXR(Device& device, const std::string& filepath);

    // Create a VTEX from an EXR on the CPU (writes VTEX container to disk). If loadIntoGpu is true
    // the resulting VTEX will be loaded into a Texture and returned; otherwise returns nullptr.
    static std::shared_ptr<Texture>
    createFromEXR_CPUOnly(Device& device, const std::string& exrPath, const std::string& outVtexPath, bool loadIntoGpu = false, VkFormat targetFormat = VK_FORMAT_R32G32B32A32_SFLOAT);

    // Load engine VTEX container produced by baking pipeline (fast GPU-ready container)
    static std::shared_ptr<Texture> createFromVTEX(Device& device, const std::string& filepath);

  public:
    // Adopted handle constructor (used by VTEX loader)
    [[nodiscard]] VkImageView           getImageView() const { return imageView_; }
    [[nodiscard]] VkSampler             getSampler() const { return sampler_; }
    [[nodiscard]] VkImage               getImage() const { return image_; }
    [[nodiscard]] VkDescriptorImageInfo getDescriptorInfo() const
    {
      return VkDescriptorImageInfo{
              .sampler     = sampler_,
              .imageView   = imageView_,
              .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
      };
    }

    [[nodiscard]] int getWidth() const { return width_; }
    [[nodiscard]] int getHeight() const { return height_; }
    [[nodiscard]] int getMipLevels() const { return mipLevels_; }

    void                   setGlobalIndex(uint32_t index) { globalIndex_ = index; }
    [[nodiscard]] uint32_t getGlobalIndex() const { return globalIndex_; }

    // CPU-only load path: read VTEX header and create a Texture-like object without allocating GPU resources.
    // Used by unit tests to validate metadata without requiring a Vulkan device.
    static std::shared_ptr<Texture> createFromVTEX(Device& device, const std::string& filepath, bool cpuOnly);

    /**
     * @brief Get approximate memory size of this texture
     * @return Memory size in bytes (includes mipmaps)
     */
    [[nodiscard]] size_t getMemorySize() const;

  private:
    // Private constructor for adopting Vulkan handles created externally (used by VTEX loader)
    Texture(Device& device, VkImage image, VkDeviceMemory memory, VkImageView view, VkSampler sampler, int width, int height, uint32_t mipLevels, VkFormat format);

    // Private constructor for creating textures from memory
    Texture(Device& device, const unsigned char* pixels, int width, int height, VkFormat format);
    // Private constructor for creating float RGBA textures (EXR loader)
    Texture(Device& device, const float* pixels, int width, int height, VkFormat format);
    // Private constructor for cpu-only metadata-only Texture (no GPU resources)
    Texture(Device& device, int width, int height, uint32_t mipLevels, VkFormat format, bool cpuOnly);

    void createImage(int width, int height, uint32_t mipLevels, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties);
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

    // True when this Texture instance was created in CPU-only mode and does not own Vulkan resources
    bool cpuOnly_ = false;
  };

} // namespace engine

#endif // VULKANENGINE_INCLUDE_ENGINE_RESOURCES_TEXTURE_HPP
