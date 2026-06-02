#pragma once

#include "Engine/Application/Ports/ICompositionPort.hpp"

namespace engine {

class EngineState;
class PostProcessPushConstants;

// Adapter that bridges EngineState to the composition port.
class CompositionAdapter final : public ICompositionPort {
 public:
  explicit CompositionAdapter(EngineState& engineState);

  [[nodiscard]] PostProcessingSystem* getPostProcessingSystem() override;
  [[nodiscard]] PostProcessPushConstants& getPostProcessPush() override;
  [[nodiscard]] VkDescriptorSet getPostProcessDescriptorSet(uint32_t frameIndex) override;
  void renderPostProcessing(FrameInfo& frameInfo, VkDescriptorSet descriptorSet, PostProcessPushConstants& push) override;
  void renderUI(FrameInfo& frameInfo, VkCommandBuffer commandBuffer, bool enabled) override;

 private:
  EngineState& engineState_;
};

}  // namespace engine
