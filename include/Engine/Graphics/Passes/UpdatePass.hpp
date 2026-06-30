#pragma once

#include "Engine/Graphics/FrameGraph/RenderGraph.hpp"

namespace engine {

    class ObjectSelectionSystem;
    class InputSystem;
    class JoltPhysicsSystem;
    class Renderer;

    class UpdatePass : public IRenderPass {
       public:
        UpdatePass(ObjectSelectionSystem* objSel, InputSystem* input,
            JoltPhysicsSystem* jolt, bool& physicsRunning,
            Renderer& renderer);

        void                             execute(FrameInfo& frameInfo) override;
        [[nodiscard]] const std::string& getName() const override;

       private:
        ObjectSelectionSystem* objSel_ = nullptr;
        InputSystem*           input_  = nullptr;
        JoltPhysicsSystem*     jolt_   = nullptr;
        bool&                  physRunning_;
        Renderer&              renderer_;
    };

}  // namespace engine
