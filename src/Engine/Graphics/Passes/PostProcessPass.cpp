#include "Engine/Graphics/Passes/PostProcessPass.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_inverse.hpp>

#include "Engine/EngineState.hpp"
#include "Engine/Graphics/FrameInfo.hpp"
#include "Engine/Graphics/Renderer.hpp"
#include "Engine/Systems/PostProcessingSystem.hpp"
namespace engine {
    PostProcessPass::PostProcessPass(Renderer& renderer, EngineState& engineState)
        : RenderPassBase("PostProcess"), renderer_(renderer), engineState_(engineState) {}
    void PostProcessPass::execute(FrameInfo& frameInfo) {
        auto& pp            = engineState_.system<PostProcessingSystem>();
        auto  descriptorSet = engineState_.postProcessDescriptorSet(frameInfo.frameIndex);
        // Populate push constants from engine state + camera (for SSAO depth reconstruction)
        PostProcessPushConstants push = engineState_.postProcess();
        push.inverseProjection        = glm::inverse(frameInfo.camera.getProjection());
        push.projection               = frameInfo.camera.getProjection();
        // Update post-process descriptors to current offscreen color/depth images
        // (in case of resize since last frame)
        engineState_.updatePostProcessDescriptors(frameInfo.frameIndex, renderer_);
        // Begin post-fx render pass and draw the full-screen quad
        renderer_.beginPostFxRenderPass(frameInfo.commandBuffer);
        pp.render(frameInfo, descriptorSet, push);
        renderer_.endOffscreenRenderPass(frameInfo.commandBuffer);
    }
}  // namespace engine