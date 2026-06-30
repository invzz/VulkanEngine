#pragma once

#include "Engine/Graphics/FrameGraph/RenderGraph.hpp"

namespace engine {

    class EngineState;
    class DeferredLightingSystem;
    class ShadowSystem;
    class Renderer;
    class Device;
    class IRenderContextPort;

    /// Deferred lighting pass.
    class DeferredLightingPass : public IRenderPass {
       public:
        DeferredLightingPass(DeferredLightingSystem& deferred, ShadowSystem& shadow,
            EngineState& engine, Renderer& renderer,
            Device& device, IRenderContextPort& renderCtx);

        void                             execute(FrameInfo& frameInfo) override;
        [[nodiscard]] const std::string& getName() const override;

       private:
        void updateShadowDescriptors(int frameIndex);

        DeferredLightingSystem& deferred_;
        ShadowSystem&           shadow_;
        EngineState&            engine_;
        Renderer&               renderer_;
        Device&                 device_;
        IRenderContextPort&     renderCtx_;
    };

}  // namespace engine
