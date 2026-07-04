#include "Engine/Graphics/Passes/ComputePass.hpp"

#include "Engine/Systems/AnimationSystem.hpp"
namespace engine {
    ComputePass::ComputePass(AnimationSystem* animationSystem)
        : RenderPassBase("Compute"), animationSystem_(animationSystem) {}
    void ComputePass::execute(FrameInfo& frameInfo) {
        if (animationSystem_ != nullptr) {
            animationSystem_->update(frameInfo);
        }
    }
}  // namespace engine
