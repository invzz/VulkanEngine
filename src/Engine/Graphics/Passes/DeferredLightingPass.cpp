#include "Engine/Graphics/Passes/DeferredLightingPass.hpp"

#include "Engine/EngineState.hpp"
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
        : deferred_(deferred), shadow_(shadow), engine_(engine), renderer_(renderer), device_(device), renderCtx_(renderCtx) {}

    void DeferredLightingPass::execute(FrameInfo& frameInfo) {
        int fi                        = frameInfo.frameIndex;
        frameInfo.globalDescriptorSet = renderCtx_.getGlobalDescriptorSet(fi);

        renderer_.beginDeferredLightingRenderPass(frameInfo.commandBuffer);
        updateShadowDescriptors(fi);

        deferred_.render(frameInfo,
            frameInfo.globalDescriptorSet,
            engine_.gbufferDescriptorSet(fi),
            engine_.deferredShadowDescriptorSet(fi),
            engine_.deferredIblDescriptorSet(fi));

        renderer_.endOffscreenRenderPass(frameInfo.commandBuffer);
    }

    void DeferredLightingPass::updateShadowDescriptors(int fi) {
        int sc = shadow_.getShadowLightCount();
        int cc = shadow_.getCubeShadowLightCount();

        std::array<VkDescriptorImageInfo, ShadowSystem::MAX_SHADOW_MAPS> si{};
        for (int i = 0; i < sc && i < ShadowSystem::MAX_SHADOW_MAPS; i++)
            si[i] = shadow_.getShadowMapDescriptorInfo(i);
        for (int i = sc; i < ShadowSystem::MAX_SHADOW_MAPS; i++)
            si[i] = shadow_.getShadowMapDescriptorInfo(0);

        std::array<VkDescriptorImageInfo, ShadowSystem::MAX_CUBE_SHADOW_MAPS> ci{};
        for (int i = 0; i < cc && i < ShadowSystem::MAX_CUBE_SHADOW_MAPS; i++)
            ci[i] = shadow_.getCubeShadowMapDescriptorInfo(i);
        for (int i = cc; i < ShadowSystem::MAX_CUBE_SHADOW_MAPS; i++)
            ci[i] = shadow_.getCubeShadowMapDescriptorInfo(0);

        VkDescriptorSet                     ds = engine_.deferredShadowDescriptorSet(fi);
        std::array<VkWriteDescriptorSet, 2> w{};
        w[0].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w[0].dstSet          = ds;
        w[0].dstBinding      = 0;
        w[0].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        w[0].descriptorCount = ShadowSystem::MAX_SHADOW_MAPS;
        w[0].pImageInfo      = si.data();

        w[1].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w[1].dstSet          = ds;
        w[1].dstBinding      = 1;
        w[1].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        w[1].descriptorCount = ShadowSystem::MAX_CUBE_SHADOW_MAPS;
        w[1].pImageInfo      = ci.data();

        vkUpdateDescriptorSets(device_.device(), 2, w.data(), 0, nullptr);
    }

    const std::string& DeferredLightingPass::getName() const {
        static std::string n = "DeferredLighting";
        return n;
    }

}  // namespace engine
