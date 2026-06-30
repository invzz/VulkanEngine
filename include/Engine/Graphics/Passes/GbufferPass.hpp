#pragma once

#include "Engine/Graphics/FrameGraph/RenderGraph.hpp"

namespace engine {

    class EngineState;
    class ModelRenderSystem;
    class Renderer;
    class IRenderContextPort;

    /// G-buffer rendering pass.
    class GbufferPass : public IRenderPass {
       public:
        GbufferPass(ModelRenderSystem& models, EngineState& engine,
            Renderer& renderer, IRenderContextPort& renderCtx);

        void                             execute(FrameInfo& frameInfo) override;
        [[nodiscard]] const std::string& getName() const override;

       private:
        void refreshGbufferDescriptors(int frameIndex);

        ModelRenderSystem&  models_;
        EngineState&        engine_;
        Renderer&           renderer_;
        IRenderContextPort& renderCtx_;
    };

}  // namespace engine
