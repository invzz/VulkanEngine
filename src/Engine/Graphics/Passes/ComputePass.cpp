
#include "Engine/Graphics/Passes/ComputePass.hpp"

#include "Engine/EngineState.hpp"

namespace engine {

void ComputePass::execute(FrameInfo& frameInfo) {
  if (engineState_ != nullptr && engineState_->getAnimationSystem() != nullptr) engineState_->getAnimationSystem()->update(frameInfo);
}

}  // namespace engine