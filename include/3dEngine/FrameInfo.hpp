#pragma once

#include <vulkan/vulkan.h>

#include "Camera.hpp"

namespace engine {

  struct FrameInfo
  {
    int             frameIndex;
    float           frameTime;
    VkCommandBuffer commandBuffer;
    Camera&         camera;
    VkDescriptorSet globalDescriptorSet;
  };

} // namespace engine