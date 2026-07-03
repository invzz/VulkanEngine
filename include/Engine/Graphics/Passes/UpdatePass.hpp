#pragma once

#include "Engine/Graphics/FrameGraph/RenderGraph.hpp"

namespace engine {

    class ObjectSelectionSystem;
    class InputSystem;
    class JoltPhysicsSystem;
    class Renderer;

    class UpdatePass : public RenderPassBase {
       public:
        UpdatePass(ObjectSelectionSystem* objSel, InputSystem* input,
            JoltPhysicsSystem* jolt, bool& physicsRunning,
            Renderer& renderer);

        void execute(FrameInfo& frameInfo) override;

       private:
        ObjectSelectionSystem* objSel_ = nullptr;
        InputSystem*           input_  = nullptr;
        JoltPhysicsSystem*     jolt_   = nullptr;
        bool&                  physRunning_;
        Renderer&              renderer_;
    };

}  // namespace engine
