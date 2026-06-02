#include "Engine/Graphics/Passes/OffscreenPass.hpp"

#include "Engine/Graphics/Descriptors.hpp"
#include "Engine/Graphics/Device.hpp"
#include "Engine/Graphics/Renderer.hpp"
#include "Engine/Systems/DeferredLightingSystem.hpp"
#include "Engine/Systems/ModelRenderSystem.hpp"
#include "Engine/Systems/ShadowSystem.hpp"

#include "Editor/RenderContext.hpp"

namespace {
    void updateShadowDescriptors(engine::EngineState* engineState, engine::Device& device, int frameIndex) {
        // Shadow descriptors update
        int const shadowCount     = engineState->getShadowSystem()->getShadowLightCount();
        int const cubeShadowCount = engineState->getShadowSystem()->getCubeShadowLightCount();

        std::array<VkDescriptorImageInfo, engine::ShadowSystem::MAX_SHADOW_MAPS> shadowInfos{};
        for (int i = 0; i < shadowCount && i < engine::ShadowSystem::MAX_SHADOW_MAPS; i++) {
            shadowInfos[i] = engineState->getShadowSystem()->getShadowMapDescriptorInfo(i);
        }
        for (int i = shadowCount; i < engine::ShadowSystem::MAX_SHADOW_MAPS; i++) {
            shadowInfos[i] = engineState->getShadowSystem()->getShadowMapDescriptorInfo(0);
        }

        std::array<VkDescriptorImageInfo, engine::ShadowSystem::MAX_CUBE_SHADOW_MAPS> cubeShadowInfos{};
        for (int i = 0; i < cubeShadowCount && i < engine::ShadowSystem::MAX_CUBE_SHADOW_MAPS; i++) {
            cubeShadowInfos[i] = engineState->getShadowSystem()->getCubeShadowMapDescriptorInfo(i);
        }
        for (int i = cubeShadowCount; i < engine::ShadowSystem::MAX_CUBE_SHADOW_MAPS; i++) {
            cubeShadowInfos[i] = engineState->getShadowSystem()->getCubeShadowMapDescriptorInfo(0);
        }

        std::array<VkWriteDescriptorSet, 2> descriptorWrites{};

        descriptorWrites[0].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[0].dstSet          = engineState->deferredShadowDescriptorSetRef(frameIndex);
        descriptorWrites[0].dstBinding      = 0;
        descriptorWrites[0].dstArrayElement = 0;
        descriptorWrites[0].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrites[0].descriptorCount = engine::ShadowSystem::MAX_SHADOW_MAPS;
        descriptorWrites[0].pImageInfo      = shadowInfos.data();

        descriptorWrites[1].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[1].dstSet          = engineState->deferredShadowDescriptorSetRef(frameIndex);
        descriptorWrites[1].dstBinding      = 1;
        descriptorWrites[1].dstArrayElement = 0;
        descriptorWrites[1].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrites[1].descriptorCount = engine::ShadowSystem::MAX_CUBE_SHADOW_MAPS;
        descriptorWrites[1].pImageInfo      = cubeShadowInfos.data();

        vkUpdateDescriptorSets(device.device(), static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
    }
}  // namespace

namespace engine {

    void OffscreenPass::execute(FrameInfo& frameInfo) {
        // Reset per-frame dynamic offsets before any mesh passes.
        engineState_->getModelRenderSystem()->beginFrame(frameInfo.frameIndex);
        engineState_->getModelRenderSystem()->updateSceneColorDescriptor(frameInfo.frameIndex, renderer_.getSceneColorImageInfo(frameInfo.frameIndex));

        // Refresh G-buffer descriptors every frame (images may change on resize)
        refreshGbufferDescriptors(frameInfo.frameIndex);

        auto const prevGlobalSet      = frameInfo.globalDescriptorSet;
        frameInfo.globalDescriptorSet = engineState_->getRenderContext().getGlobalDescriptorSet(frameInfo.frameIndex);

        // Begin G-buffer with secondary command buffer support when model recording is multithreaded.
        renderer_.beginGbufferRenderPass(frameInfo.commandBuffer, engineState_->getModelRenderSystem()->isMultiThreadedRecordingEnabled());
        engineState_->getModelRenderSystem()->renderGbuffer(frameInfo);
        renderer_.endOffscreenRenderPass(frameInfo.commandBuffer);
        renderer_.beginDeferredLightingRenderPass(frameInfo.commandBuffer);

        // Update shadow descriptors
        updateShadowDescriptors(engineState_, device_, frameInfo.frameIndex);

        engineState_->getDeferredLightingSystem()->render(frameInfo,
            frameInfo.globalDescriptorSet,
            engineState_->getGbufferDescriptorSet(frameInfo.frameIndex),
            engineState_->getDeferredShadowDescriptorSet(frameInfo.frameIndex),
            engineState_->getDeferredIblDescriptorSet(frameInfo.frameIndex));
        renderer_.endOffscreenRenderPass(frameInfo.commandBuffer);

        if (debugMode_ == 0) {
            // Copy color -> scene color, then perform transmission/alpha pass
            renderer_.copyOffscreenColorToSceneColor(frameInfo.commandBuffer);

            renderer_.beginOffscreenRenderPassLoadColorDepth(frameInfo.commandBuffer);
            if ((engineState_->getGridRenderSystem() != nullptr) && engineState_->showGridRef()) {
                engineState_->getGridRenderSystem()->render(frameInfo);
            }
            engineState_->getModelRenderSystem()->renderTransmission(frameInfo);
            engineState_->getModelRenderSystem()->renderAlphaBlend(frameInfo);
            if ((engineState_->showDebugObjectsRef()) && (engineState_->getLightSystem() != nullptr)) {
                engineState_->getLightSystem()->render(frameInfo);
            }
            if ((engineState_->showDebugObjectsRef()) && (engineState_->getCameraSystem() != nullptr)) {
                engineState_->getCameraSystem()->render(frameInfo);
            }
            if ((engineState_->showColliderWireframesRef()) && (engineState_->getColliderDebugRenderSystem() != nullptr)) {
                engineState_->getColliderDebugRenderSystem()->render(frameInfo);
            }

            renderer_.endOffscreenRenderPass(frameInfo.commandBuffer);
        }

        frameInfo.globalDescriptorSet = prevGlobalSet;

        renderer_.generateOffscreenMipmaps(frameInfo.commandBuffer);
    }

    void OffscreenPass::refreshGbufferDescriptors(int frameIndex) {
        auto nInfo     = renderer_.getGbufferNormalImageInfo(frameIndex);
        auto aInfo     = renderer_.getGbufferAlbedoImageInfo(frameIndex);
        auto mInfo     = renderer_.getGbufferMaterialImageInfo(frameIndex);
        auto dInfo     = renderer_.getDepthImageInfo(frameIndex);

        DescriptorWriter(engineState_->gbufferSetLayoutRef(), engineState_->gbufferPoolRef())
            .writeImage(0, &nInfo)
            .writeImage(1, &aInfo)
            .writeImage(2, &mInfo)
            .writeImage(3, &dInfo)
            .overwrite(engineState_->gbufferDescriptorSetRef(frameIndex));
    }

}  // namespace engine
