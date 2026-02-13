#include "Engine/Graphics/Passes/HZBPass.hpp"

#include "Engine/EngineState.hpp"
#include "Engine/Graphics/Renderer.hpp"

namespace engine {

void HZBPass::execute(FrameInfo& frameInfo) {
  // HZB generation currently only needs the renderer, but EngineState is
  // accepted for future access to HZB-related settings or resources.
  renderer_.generateDepthPyramid(frameInfo.commandBuffer);
}

}  // namespace engine
