#pragma once

#include "Engine/Graphics/FrameGraph/RenderGraph.hpp"

namespace engine {

    class ModelRenderSystem;
    class Renderer;

    class DepthPrepass : public IRenderPass {
       public:
        DepthPrepass(ModelRenderSystem& models, Renderer& renderer);

        void                             execute(FrameInfo& frameInfo) override;
        [[nodiscard]] const std::string& getName() const override;

       private:
        ModelRenderSystem& models_;
        Renderer&          renderer_;
    };

}  // namespace engine
