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

  // Recreate using the existing post-process set layout from EngineState.
  // Convenience method so delivery doesn't need to access postProcessSetLayoutRef().
  virtual void recreatePostProcessingSystemWithExistingLayout(Device& device,
                                                              VkRenderPass renderPass) = 0;
};

}  // namespace engine
