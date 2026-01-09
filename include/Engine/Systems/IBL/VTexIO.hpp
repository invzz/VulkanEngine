#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>
#include <string>

namespace engine {
  class Device;
}

namespace engine::ibl_detail::vtex {

  struct Header
  {
    uint32_t magic      = 0x58455456; // 'VTEX'
    uint32_t version    = 1;
    uint32_t vkFormat   = 0;
    uint32_t width      = 0;
    uint32_t height     = 0;
    uint32_t mipLevels  = 0;
    uint32_t layers     = 0;
    uint32_t bytesPerPx = 0;
  };

  uint32_t bytesPerPixelFor(VkFormat format);

  bool writeImage(Device& device, const std::string& filePath, VkImage image, VkFormat format, uint32_t width, uint32_t height, uint32_t mipLevels, uint32_t layers);

  bool loadImage(Device&            device,
                 const std::string& filePath,
                 VkImage&           image,
                 VkDeviceMemory&    memory,
                 VkImageView&       view,
                 VkSampler&         sampler,
                 VkImageViewType    viewType,
                 VkImageCreateFlags flags,
                 Header*            outHeader = nullptr);

  // Read only the VTEX header and return true if the file is a valid VTEX container.
  bool readHeader(const std::string& filePath, Header& outHeader);

} // namespace engine::ibl_detail::vtex
