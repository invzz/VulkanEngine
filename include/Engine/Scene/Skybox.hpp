#pragma once

#include <vulkan/vulkan.h>

#include <array>
#include <memory>
#include <string>

#include "Engine/Graphics/Device.hpp"

namespace engine {

  /**
   * @brief Cubemap texture for skybox rendering
   *
   * Loads 6 face textures (right, left, top, bottom, front, back) into a Vulkan cubemap.
   * Supports JPG, PNG, and other formats via stb_image.
   */
  class Skybox
  {
  public:
    /**
     * @brief Load skybox from 6 separate face images
     * @param device Vulkan device
     * @param facePaths Array of 6 paths: [+X, -X, +Y, -Y, +Z, -Z]
     *                  (right, left, top, bottom, front, back)
     */
    Skybox(Device& device, const std::array<std::string, 6>& facePaths);

    /**
     * @brief Load skybox from folder with standard naming
     * @param device Vulkan device
     * @param folderPath Folder containing posx.jpg, negx.jpg, etc.
     * @param extension File extension (default: "jpg")
     */
    static std::unique_ptr<Skybox> loadFromFolder(Device& device, const std::string& folderPath, const std::string& extension = "jpg");

    ~Skybox();

    Skybox(const Skybox&)            = delete;
    Skybox& operator=(const Skybox&) = delete;
    Skybox(Skybox&&)                 = delete;
    Skybox& operator=(Skybox&&)      = delete;

    VkImageView getImageView() const { return imageView_; }
    VkSampler   getSampler() const { return sampler_; }

    VkDescriptorImageInfo getDescriptorInfo() const
    {
      return VkDescriptorImageInfo{
              .sampler     = sampler_,
              .imageView   = imageView_,
              .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
      };
    }

    int getSize() const { return size_; }

  private:
    void createCubemapImage(const std::array<std::string, 6>& facePaths);
    void createImageView();
    void createSampler();
    void transitionImageLayout(VkImageLayout oldLayout, VkImageLayout newLayout);
    void copyBufferToImage(VkBuffer buffer, uint32_t faceIndex);

    Device& device_;

    VkImage        image_       = VK_NULL_HANDLE;
    VkDeviceMemory imageMemory_ = VK_NULL_HANDLE;
    VkImageView    imageView_   = VK_NULL_HANDLE;
    VkSampler      sampler_     = VK_NULL_HANDLE;

    int size_ = 0; // Width/height of each face (assumed square)
  };

} // namespace engine
