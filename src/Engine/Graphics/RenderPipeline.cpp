#include "Engine/Graphics/RenderPipeline.hpp"

#include "Engine/Graphics/FrameGraph/RenderGraph.hpp"

namespace engine {
    RenderPipeline::RenderPipeline(Renderer& r) : renderer(r) {}

    void RenderPipeline::setRenderGraph(std::unique_ptr<RenderGraph> graph) {
        renderGraph = std::move(graph);
    }

    void RenderPipeline::execute(FrameInfo& frameInfo) {
        if (renderGraph) {
            renderGraph->execute(frameInfo);
        }
    }
}  // namespace engine