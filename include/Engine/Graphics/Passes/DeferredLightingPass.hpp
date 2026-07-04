#pragma once
#include "Engine/Graphics/FrameGraph/RenderGraph.hpp"
namespace engine {
    class EngineState;
    class DeferredLightingSystem;
    class ShadowSystem;
    class Renderer;
    class Device;
    class IRenderContextPort;
    class DeferredLightingPass : public RenderPassBase {
       public:
        DeferredLightingPass(DeferredLightingSystem& deferred, ShadowSystem& shadow,
            EngineState& engine, Renderer& renderer,
            Device& device, IRenderContextPort& renderCtx);
        void execute(FrameInfo& frameInfo) override;

       private:
        void                    updateShadowDescriptors(int frameIndex);
        DeferredLightingSystem& deferred_;
        ShadowSystem&           shadow_;
        EngineState&            engine_;
        Renderer&               renderer_;
        Device&                 device_;
        IRenderContextPort&     renderCtx_;
    };
}  // namespace engine
