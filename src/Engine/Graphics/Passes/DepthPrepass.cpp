#include "Engine/Graphics/Passes/DepthPrepass.hpp"

#include "Engine/EngineState.hpp"
#include "Engine/Graphics/Renderer.hpp"

namespace engine {

void DepthPrepass::execute(FrameInfo& frameInfo) {
  renderer_.beginOffscreenDepthPrepassRenderPass(frameInfo.commandBuffer);
  if (engineState_ != nullptr && engineState_->getModelRenderSystem() != nullptr) engineState_->getModelRenderSystem()->renderDepthPrepass(frameInfo);
  renderer_.endOffscreenRenderPass(frameInfo.commandBuffer);
}

}  // namespace engine
