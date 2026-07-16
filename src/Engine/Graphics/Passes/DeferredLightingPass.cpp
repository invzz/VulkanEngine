#include "Engine/Graphics/Passes/DeferredLightingPass.hpp"

#include "Engine/EngineState.hpp"
#include "Engine/Graphics/Descriptors.hpp"
#include "Engine/Graphics/Device.hpp"
#include "Engine/Graphics/IRenderContextPort.hpp"
#include "Engine/Graphics/Renderer.hpp"
#include "Engine/Systems/DeferredLightingSystem.hpp"
#include "Engine/Systems/ShadowSystem.hpp"
namespace engine {
    DeferredLightingPass::DeferredLightingPass(
        DeferredLightingSystem& deferred, ShadowSystem& shadow,
        EngineState& engine, Renderer& renderer,
        Device& device, IRenderContextPort& renderCtx)
        : RenderPassBase("DeferredLighting"), deferred_(deferred), shadow_(shadow), engine_(engine), renderer_(renderer), device_(device), renderCtx_(renderCtx) {}
    void DeferredLightingPass::execute(FrameInfo& frameInfo) {
        int fi                        = frameInfo.frameIndex;
        frameInfo.globalDescriptorSet = renderCtx_.getGlobalDescriptorSet(fi);
        renderer_.beginDeferredLightingRenderPass(frameInfo.commandBuffer);
        updateShadowDescriptors(fi);
        deferred_.render(frameInfo,
            frameInfo.globalDescriptorSet,
            engine_.descriptors().gbufferDescriptorSet(fi),
            engine_.descriptors().deferredShadowDescriptorSet(fi),
            engine_.descriptors().deferredIblDescriptorSet(fi));
        renderer_.endOffscreenRenderPass(frameInfo.commandBuffer);
    }
    void DeferredLightingPass::updateShadowDescriptors(int fi) {
        int                                                              sc = shadow_.getShadowLightCount();
        int                                                              cc = shadow_.getCubeShadowLightCount();
        std::array<VkDescriptorImageInfo, ShadowSystem::MAX_SHADOW_MAPS> si{};
        for (int i = 0; i < sc && i < ShadowSystem::MAX_SHADOW_MAPS; i++) {
            si[i] = shadow_.getShadowMapDescriptorInfo(i);
        }
        for (int i = sc; i < ShadowSystem::MAX_SHADOW_MAPS; i++) {
            si[i] = shadow_.getShadowMapDescriptorInfo(0);
        }
        std::array<VkDescriptorImageInfo, ShadowSystem::MAX_CUBE_SHADOW_MAPS> ci{};
        for (int i = 0; i < cc && i < ShadowSystem::MAX_CUBE_SHADOW_MAPS; i++) {
            ci[i] = shadow_.getCubeShadowMapDescriptorInfo(i);
        }
        for (int i = cc; i < ShadowSystem::MAX_CUBE_SHADOW_MAPS; i++) {
            ci[i] = shadow_.getCubeShadowMapDescriptorInfo(0);
        }
        DescriptorWriter(engine_.descriptors().deferredShadowSetLayout(), engine_.descriptors().deferredShadowPool())
            .writeImageArray(0, si.data(), ShadowSystem::MAX_SHADOW_MAPS)
            .writeImageArray(1, ci.data(), ShadowSystem::MAX_CUBE_SHADOW_MAPS)
            .overwrite(engine_.descriptors().deferredShadowDescriptorSetRef(fi));
    }
}  // namespace engine
