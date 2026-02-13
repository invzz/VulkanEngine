
#include "Engine/Graphics/Passes/ComputePass.hpp"

#include "Engine/EngineState.hpp"

namespace engine {

void ComputePass::execute(FrameInfo& frameInfo) {
  if (engineState_ != nullptr && engineState_->animationSystem) engineState_->animationSystem->update(frameInfo);
}

}  // namespace engine