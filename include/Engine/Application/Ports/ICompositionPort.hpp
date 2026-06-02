#pragma once

#include <cstdint>
#include <vulkan/vulkan_core.h>

namespace engine {

struct FrameInfo;
class PostProcessingSystem;
class UIManager;
class PostProcessPushConstants;

// Port for composition pass without knowing EngineState internals.
class ICompositionPort {
 public:
  virtual ~ICompositionPort() = default;

  [[nodiscard]] virtual PostProcessingSystem* getPostProcessingSystem() = 0;
  [[nodiscard]] virtual PostProcessPushConstants& getPostProcessPush() = 0;
  [[nodiscard]] virtual VkDescriptorSet getPostProcessDescriptorSet(uint32_t frameIndex) = 0;
  [[nodiscard]] virtual void renderPostProcessing(FrameInfo& frameInfo, VkDescriptorSet descriptorSet, PostProcessPushConstants& push) = 0;
  [[nodiscard]] virtual void renderUI(FrameInfo& frameInfo, VkCommandBuffer commandBuffer, bool enabled) = 0;
};

}  // namespace engine
