
#include "Engine/Graphics/Passes/ComputePass.hpp"
#include "Engine/Systems/AnimationSystem.hpp"

namespace engine {

void ComputePass::execute(FrameInfo& frameInfo) {
  if (animationPort_ != nullptr) {
    auto* animation = animationPort_->getAnimationSystem();
    if (animation != nullptr) animation->update(frameInfo);
  }
}

}  // namespace engine