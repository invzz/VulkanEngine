
#ifndef RENDER_PIPELINE_HPP
#define RENDER_PIPELINE_HPP

#include "Engine/Graphics/FrameGraph/RenderGraph.hpp"
#include "Engine/Graphics/FrameInfo.hpp"
#include "Engine/Graphics/Renderer.hpp"
namespace engine {

    class RenderPipeline {
       public:
        explicit RenderPipeline(Renderer& renderer);

        void setRenderGraph(std::unique_ptr<RenderGraph> graph);
        void execute(FrameInfo& frameInfo);

       private:
        Renderer&                    renderer;
        std::unique_ptr<RenderGraph> renderGraph;
    };
}  // namespace engine
#endif  // RENDER_PIPELINE_HPP