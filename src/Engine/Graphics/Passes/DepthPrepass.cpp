#include "Engine/Graphics/Passes/DepthPrepass.hpp"

#include "Engine/EngineState.hpp"
#include "Engine/Graphics/Renderer.hpp"

namespace engine {

void DepthPrepass::execute(FrameInfo& frameInfo) {
  auto rendering = engineState_->renderingService().view();

  renderer_.beginOffscreenDepthPrepassRenderPass(frameInfo.commandBuffer);
  if (engineState_ != nullptr && rendering.modelRenderSystem != nullptr) rendering.modelRenderSystem->renderDepthPrepass(frameInfo);
  renderer_.endOffscreenRenderPass(frameInfo.commandBuffer);
}

}  // namespace engine
