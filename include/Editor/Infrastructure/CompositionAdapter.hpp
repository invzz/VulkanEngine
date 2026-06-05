#pragma once

#include "Engine/Application/Ports/ICompositionPort.hpp"

namespace engine {

class EngineState;
class UIManager;

// Adapter that bridges EngineState to the composition port.
class CompositionAdapter final : public ICompositionPort {
 public:
  CompositionAdapter(EngineState& engineState, UIManager* uiManager, bool drawUI = true);

  [[nodiscard]] PostProcessingSystem* getPostProcessingSystem() override;
  [[nodiscard]] PostProcessPushConstants& getPostProcessPush() override;
  [[nodiscard]] VkDescriptorSet getPostProcessDescriptorSet(uint32_t frameIndex) override;
  void renderPostProcessing(FrameInfo& frameInfo, VkDescriptorSet descriptorSet, PostProcessPushConstants& push) override;
  void renderUI(FrameInfo& frameInfo, VkCommandBuffer commandBuffer, bool enabled) override;

 private:
  EngineState& engineState_;
  UIManager* uiManager_ = nullptr;
  bool drawUI_ = true;
};

}  // namespace engine
