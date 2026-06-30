#pragma once

#include "Engine/Graphics/FrameGraph/RenderGraph.hpp"

namespace engine {

    class AnimationSystem;

    class ComputePass : public IRenderPass {
       public:
        explicit ComputePass(AnimationSystem* animationSystem);

        void                             execute(FrameInfo& frameInfo) override;
        [[nodiscard]] const std::string& getName() const override {
            static std::string name = "Compute";
            return name;
        }

       private:
        AnimationSystem* animationSystem_ = nullptr;
    };

}  // namespace engine
