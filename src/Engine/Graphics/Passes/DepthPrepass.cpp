#include "Engine/Graphics/Passes/DepthPrepass.hpp"

#include "Engine/Graphics/Renderer.hpp"
#include "Engine/Systems/ModelRenderSystem.hpp"
namespace engine {
    DepthPrepass::DepthPrepass(ModelRenderSystem& models, Renderer& renderer)
        : RenderPassBase("DepthPrepass"), models_(models), renderer_(renderer) {}
    void DepthPrepass::execute(FrameInfo& frameInfo) {
        renderer_.beginOffscreenDepthPrepassRenderPass(frameInfo.commandBuffer);
        models_.renderDepthPrepass(frameInfo);
        renderer_.endOffscreenRenderPass(frameInfo.commandBuffer);
    }
}  // namespace engine
