#pragma once

#include "Engine/Graphics/FrameGraph/RenderGraph.hpp"

namespace engine {

    class EngineState;
    class ModelRenderSystem;
    class Renderer;
    class IRenderContextPort;

    class GbufferPass : public RenderPassBase {
       public:
        GbufferPass(ModelRenderSystem& models, EngineState& engine,
            Renderer& renderer, IRenderContextPort& renderCtx);

        void execute(FrameInfo& frameInfo) override;

       private:
        void refreshGbufferDescriptors(int frameIndex);

        ModelRenderSystem&  models_;
        EngineState&        engine_;
        Renderer&           renderer_;
        IRenderContextPort& renderCtx_;
    };

}  // namespace engine
