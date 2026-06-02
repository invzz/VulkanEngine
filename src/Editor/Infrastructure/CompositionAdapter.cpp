#include "Editor/Infrastructure/CompositionAdapter.hpp"

#include "Engine/EngineState.hpp"
#include "Engine/Graphics/Descriptors.hpp"
#include "Engine/Systems/PostProcessingSystem.hpp"

#include "Editor/ui/UIManager.hpp"

namespace engine {

CompositionAdapter::CompositionAdapter(EngineState& engineState)
    : engineState_(engineState) {}

PostProcessingSystem* CompositionAdapter::getPostProcessingSystem() {
  return engineState_.renderingService().view().postProcessingSystem;
}

PostProcessPushConstants& CompositionAdapter::getPostProcessPush() {
  return engineState_.postProcessPushRef();
}

VkDescriptorSet CompositionAdapter::getPostProcessDescriptorSet(uint32_t frameIndex) {
  return engineState_.getPostProcessDescriptorSet(frameIndex);
}

void CompositionAdapter::renderPostProcessing(FrameInfo& frameInfo, VkDescriptorSet descriptorSet, PostProcessPushConstants& push) {
  auto* postProcessingSystem = engineState_.renderingService().view().postProcessingSystem;
  if (postProcessingSystem != nullptr) {
    postProcessingSystem->render(frameInfo, descriptorSet, push);
  }
}

void CompositionAdapter::renderUI(FrameInfo& frameInfo, VkCommandBuffer commandBuffer, bool enabled) {
  // UI rendering is handled by the UIManager which is passed separately.
  // This method is a placeholder for future UI abstraction.
  (void)frameInfo;
  (void)commandBuffer;
  (void)enabled;
}

}  // namespace engine
