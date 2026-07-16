
#ifndef RENDER_PIPELINE_HPP
#define RENDER_PIPELINE_HPP
#include <memory>

#include "Engine/Graphics/FrameGraph/RenderGraph.hpp"
#include "Engine/Graphics/FrameInfo.hpp"
#include "Engine/Graphics/Passes/CompositionPass.hpp"

namespace engine {
    class EngineState;
    class Device;
    class RenderPipeline {
       public:
        explicit RenderPipeline(Renderer& renderer);
        void       setRenderGraph(std::unique_ptr<RenderGraph> graph);
        /// Build and install the engine's default render graph (update ->
        /// compute -> shadow -> depth -> gbuffer -> deferred -> forward ->
        /// selection mask -> transition -> post-process -> selection composite
        /// -> composition/ImGui). Keeps the pass wiring owned by the pipeline
        /// instead of the Editor composition root.
        void       buildDefaultGraph(EngineState& engineState, Renderer& renderer, Device& device,
            float rtShadowSoftness, CompositionPass::UIRenderFn renderUi, class Window& window);
        void       execute(FrameInfo& frameInfo);

       private:
        Renderer&                    renderer;
        std::unique_ptr<RenderGraph> renderGraph;
    };
}  // namespace engine
#endif