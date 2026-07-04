#include "Engine/Graphics/Passes/GbufferPass.hpp"

#include "Engine/EngineState.hpp"
#include "Engine/Graphics/Descriptors.hpp"
#include "Engine/Graphics/IRenderContextPort.hpp"
#include "Engine/Graphics/Renderer.hpp"
#include "Engine/Systems/ModelRenderSystem.hpp"
namespace engine {
    GbufferPass::GbufferPass(ModelRenderSystem& models, EngineState& engine,
        Renderer& renderer, IRenderContextPort& renderCtx)
        : RenderPassBase("Gbuffer"), models_(models), engine_(engine), renderer_(renderer), renderCtx_(renderCtx) {}
    void GbufferPass::execute(FrameInfo& frameInfo) {
        models_.beginFrame(frameInfo.frameIndex);
        models_.updateSceneColorDescriptor(frameInfo.frameIndex,
            renderer_.getSceneColorImageInfo(frameInfo.frameIndex));
        refreshGbufferDescriptors(frameInfo.frameIndex);
        frameInfo.globalDescriptorSet = renderCtx_.getGlobalDescriptorSet(frameInfo.frameIndex);
        renderer_.beginGbufferRenderPass(frameInfo.commandBuffer,
            models_.isMultiThreadedRecordingEnabled());
        models_.renderGbuffer(frameInfo);
        renderer_.endOffscreenRenderPass(frameInfo.commandBuffer);
        renderer_.transitionDepthToShaderReadOnly(frameInfo.commandBuffer);
    }
    void GbufferPass::refreshGbufferDescriptors(int frameIndex) {
        auto nInfo = renderer_.getGbufferNormalImageInfo(frameIndex);
        auto aInfo = renderer_.getGbufferAlbedoImageInfo(frameIndex);
        auto mInfo = renderer_.getGbufferMaterialImageInfo(frameIndex);
        auto dInfo = renderer_.getDepthImageInfo(frameIndex);
        DescriptorWriter(engine_.gbufferSetLayout(), engine_.gbufferPool())
            .writeImage(0, &nInfo)
            .writeImage(1, &aInfo)
            .writeImage(2, &mInfo)
            .writeImage(3, &dInfo)
            .overwrite(engine_.gbufferDescriptorSetRef(frameIndex));
    }
}  // namespace engine
