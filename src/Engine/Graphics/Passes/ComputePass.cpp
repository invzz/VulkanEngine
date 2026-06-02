
#include "Engine/Graphics/Passes/ComputePass.hpp"

#include "Engine/EngineState.hpp"

namespace engine {

void ComputePass::execute(FrameInfo& frameInfo) {
  auto systems = engineState_->systemServices();
  if (engineState_ != nullptr && systems.animation != nullptr) systems.animation->update(frameInfo);
}

}  // namespace engine