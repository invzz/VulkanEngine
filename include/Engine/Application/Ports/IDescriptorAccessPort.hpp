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

  // Post-process descriptor resources
  [[nodiscard]] virtual DescriptorPool& getDescriptorPool() = 0;
  [[nodiscard]] virtual DescriptorSetLayout& getPostProcessSetLayout() = 0;
  [[nodiscard]] virtual VkDescriptorSet getPostProcessDescriptorSet(uint32_t frameIndex) = 0;
  [[nodiscard]] virtual VkDescriptorSet& postProcessDescriptorSetRef(int frameIndex) = 0;

  // G-buffer descriptor resources
  [[nodiscard]] virtual DescriptorPool& gbufferPoolRef() = 0;
  [[nodiscard]] virtual DescriptorSetLayout& gbufferSetLayoutRef() = 0;
  [[nodiscard]] virtual VkDescriptorSet getGbufferDescriptorSet(uint32_t frameIndex) = 0;
  [[nodiscard]] virtual VkDescriptorSet& gbufferDescriptorSetRef(int frameIndex) = 0;

  // Deferred shadow descriptor resources
  [[nodiscard]] virtual VkDescriptorSet getDeferredShadowDescriptorSet(uint32_t frameIndex) = 0;
  [[nodiscard]] virtual VkDescriptorSet& deferredShadowDescriptorSetRef(int frameIndex) = 0;

  // Deferred IBL descriptor resources
  [[nodiscard]] virtual VkDescriptorSet getDeferredIblDescriptorSet(uint32_t frameIndex) = 0;
};

}  // namespace engine
