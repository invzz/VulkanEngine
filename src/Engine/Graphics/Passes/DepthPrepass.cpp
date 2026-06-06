#include "Engine/Graphics/Passes/DepthPrepass.hpp"

#include "Engine/Graphics/Renderer.hpp"
#include "Engine/Systems/ModelRenderSystem.hpp"

namespace engine {

    void DepthPrepass::execute(FrameInfo& frameInfo) {
        renderer_.beginOffscreenDepthPrepassRenderPass(frameInfo.commandBuffer);
        if (rendering_.modelRenderSystem != nullptr)
            rendering_.modelRenderSystem->renderDepthPrepass(frameInfo);
        renderer_.endOffscreenRenderPass(frameInfo.commandBuffer);
    }

}  // namespace engine
