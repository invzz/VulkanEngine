#include "Engine/Graphics/Passes/CompositionPass.hpp"

#include "Engine/Core/Window.hpp"
#include "Engine/Graphics/Renderer.hpp"

namespace engine {

    CompositionPass::CompositionPass(Renderer& renderer, UIRenderFn renderUI, Window& window)
        : renderer_(renderer), renderUI_(std::move(renderUI)), window_(window) {}

    void CompositionPass::execute(FrameInfo& frameInfo) {
        renderer_.beginSwapChainRenderPass(frameInfo.commandBuffer);

        if (renderUI_) {
            renderUI_(frameInfo, frameInfo.commandBuffer, window_.isCursorVisible());
        }

        renderer_.endSwapChainRenderPass(frameInfo.commandBuffer);
    }

}  // namespace engine
