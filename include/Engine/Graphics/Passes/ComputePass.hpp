#pragma once
#include "Engine/Graphics/FrameGraph/RenderGraph.hpp"
namespace engine {
    class AnimationSystem;
    class ComputePass : public RenderPassBase {
       public:
        explicit ComputePass(AnimationSystem* animationSystem);
        void execute(FrameInfo& frameInfo) override;

       private:
        AnimationSystem* animationSystem_ = nullptr;
    };
}  // namespace engine
