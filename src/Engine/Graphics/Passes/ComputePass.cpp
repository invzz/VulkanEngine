
#include "Engine/Graphics/Passes/ComputePass.hpp"

#include "Engine/EngineState.hpp"

namespace engine {

void ComputePass::execute(FrameInfo& frameInfo) {
  if (engineState_ != nullptr) {
    auto* animation = engineState_->animationRuntimeService().animation();
    if (animation != nullptr) animation->update(frameInfo);
  }
}

}  // namespace engine