#pragma once
#include "Engine/Graphics/FrameGraph/RenderGraph.hpp"
namespace engine {
    class ModelRenderSystem;
    class Renderer;
    class DepthPrepass : public RenderPassBase {
       public:
        DepthPrepass(ModelRenderSystem& models, Renderer& renderer);
        void execute(FrameInfo& frameInfo) override;

       private:
        ModelRenderSystem& models_;
        Renderer&          renderer_;
    };
}  // namespace engine
