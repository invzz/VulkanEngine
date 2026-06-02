#pragma once

#include <cstdint>
#include <vulkan/vulkan_core.h>

namespace engine {

class DescriptorPool;
class DescriptorSetLayout;

// Port for descriptor access without knowing EngineState internals.
class IDescriptorAccessPort {
 public:
  virtual ~IDescriptorAccessPort() = default;

  [[nodiscard]] virtual DescriptorPool& getDescriptorPool() = 0;
  [[nodiscard]] virtual DescriptorSetLayout& getPostProcessSetLayout() = 0;
  [[nodiscard]] virtual VkDescriptorSet getGbufferDescriptorSet(uint32_t frameIndex) = 0;
  [[nodiscard]] virtual VkDescriptorSet getDeferredShadowDescriptorSet(uint32_t frameIndex) = 0;
  [[nodiscard]] virtual VkDescriptorSet getDeferredIblDescriptorSet(uint32_t frameIndex) = 0;
  [[nodiscard]] virtual VkDescriptorSet getPostProcessDescriptorSet(uint32_t frameIndex) = 0;
};

}  // namespace engine
