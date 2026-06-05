#include "Editor/Infrastructure/CompositionAdapter.hpp"

#include "Editor/ui/UIManager.hpp"
#include "Engine/EngineState.hpp"
#include "Engine/Graphics/Descriptors.hpp"
#include "Engine/Systems/PostProcessingSystem.hpp"

namespace engine {

CompositionAdapter::CompositionAdapter(EngineState& engineState, UIManager* uiManager)
    : engineState_(engineState), uiManager_(uiManager) {}

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
  if (uiManager_ != nullptr) {
    uiManager_->render(frameInfo, commandBuffer, enabled);
  }
}

}  // namespace engine
