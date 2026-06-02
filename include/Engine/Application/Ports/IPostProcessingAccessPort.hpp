#pragma once

#include <vector>

#include <vulkan/vulkan_core.h>

namespace engine {

class Device;

// Port for delivery to recreate the post-processing system on swapchain events
// without knowing EngineState's internals.
class IPostProcessingAccessPort {
 public:
  virtual ~IPostProcessingAccessPort() = default;

  virtual void recreatePostProcessingSystem(Device& device,
                                            VkRenderPass renderPass,
                                            std::vector<VkDescriptorSetLayout> setLayouts) = 0;
};

}  // namespace engine
