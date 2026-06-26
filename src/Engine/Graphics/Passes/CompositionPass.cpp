#include "Engine/Graphics/Passes/CompositionPass.hpp"

#include "Engine/Graphics/Renderer.hpp"
#include "Engine/Systems/PostProcessingSystem.hpp"

namespace engine {

    void CompositionPass::execute(FrameInfo& frameInfo) {
        // Begin swapchain render pass for UI only (viewport scene goes to texture, not swapchain)
        renderer_.beginSwapChainRenderPass(frameInfo.commandBuffer);

        // Render UI (including ViewportPanel, Inspector, etc.) — viewport scene is NOT rendered to swapchain
        if (compositionPort_ != nullptr) {
            compositionPort_->renderUI(frameInfo, frameInfo.commandBuffer, window_.isCursorVisible());
        }

        // End swapchain render pass started above
        renderer_.endSwapChainRenderPass(frameInfo.commandBuffer);
    }

}  // namespace engine
