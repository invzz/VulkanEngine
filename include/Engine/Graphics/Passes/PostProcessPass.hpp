#pragma once

#include "Engine/Graphics/FrameGraph/RenderGraph.hpp"

namespace engine {

    class EngineState;
    class Renderer;

    /**
     * @brief Full-screen post-processing pass.
     *
     * Renders the offscreen HDR scene color through the post-process pipeline
     * (tonemapping, bloom, FXAA, SSAO, vignette, etc.) into a dedicated LDR
     * render target (postFxTargets). This LDR target is then sampled by the
     * Viewport panel for ImGui display.
     *
     * The swap chain render pass is NOT touched — only the offscreen/post-fx
     * framebuffers are involved. CompositionPass handles the swap chain.
     */
    class PostProcessPass : public RenderPassBase {
       public:
        PostProcessPass(Renderer& renderer, EngineState& engineState);

        void execute(FrameInfo& frameInfo) override;

       private:
        Renderer&    renderer_;
        EngineState& engineState_;
    };

}  // namespace engine