#include "Engine/Graphics/Passes/OffscreenPass.hpp"

#include "Engine/Graphics/Descriptors.hpp"
#include "Engine/Graphics/Device.hpp"
#include "Engine/Graphics/Renderer.hpp"
#include "Engine/Scene/Scene.hpp"
#include "Engine/Scene/components/DirectionalLightComponent.hpp"
#include "Engine/Scene/components/TransformComponent.hpp"
#include "Engine/Systems/DeferredLightingSystem.hpp"
#include "Engine/Systems/DustRenderSystem.hpp"
#include "Engine/Systems/ModelRenderSystem.hpp"
#include "Engine/Systems/ShadowSystem.hpp"

#include "Editor/RenderContext.hpp"

namespace {
    struct SunInfo {
        glm::vec3 directionToSun{0.0f, 1.0f, 0.0f};
        glm::vec3 color{1.0f, 1.0f, 1.0f};
        float     intensity{0.0f};
        bool      valid{false};
    };

    SunInfo queryPrimaryDirectionalLightSunInfo(engine::Scene const& scene) {
        SunInfo info{};

        auto const& registry = scene.getRegistry();
        auto        view     = registry.view<engine::TransformComponent, engine::DirectionalLightComponent>();
        for (auto entity : view) {
            auto const& transform = view.get<engine::TransformComponent>(entity);
            auto const& light     = view.get<engine::DirectionalLightComponent>(entity);

            glm::vec3 const lightRayDir = glm::normalize(transform.getForwardDir());
            info.directionToSun         = -lightRayDir;
            info.color                  = light.color;
            info.intensity              = light.intensity;
            info.valid                  = true;
            break;
        }

        return info;
    }
}  // namespace

namespace engine {

    void OffscreenPass::execute(FrameInfo& frameInfo) {
        engineState_->modelRenderSystem->enableMultiThreadedRecording(true, 0);
        // Reset per-frame dynamic offsets before any mesh passes.
        engineState_->modelRenderSystem->beginFrame(frameInfo.frameIndex);
        engineState_->modelRenderSystem->updateSceneColorDescriptor(frameInfo.frameIndex, renderer_.getSceneColorImageInfo(frameInfo.frameIndex));

        // Refresh G-buffer descriptors every frame (images may change on resize)
        {
            auto nInfo     = renderer_.getGbufferNormalImageInfo(frameInfo.frameIndex);
            auto aInfo     = renderer_.getGbufferAlbedoImageInfo(frameInfo.frameIndex);
            auto mInfo     = renderer_.getGbufferMaterialImageInfo(frameInfo.frameIndex);
            auto dInfo     = renderer_.getDepthImageInfo(frameInfo.frameIndex);
            auto cInfo     = renderer_.getOffscreenImageInfo(frameInfo.frameIndex);
            auto bakedInfo = renderer_.getGbufferBakedImageInfo(frameInfo.frameIndex);

            DescriptorWriter(engineState_->gbufferSetLayoutRef(), engineState_->gbufferPoolRef())
                .writeImage(0, &nInfo)
                .writeImage(1, &aInfo)
                .writeImage(2, &mInfo)
                .writeImage(3, &dInfo)
                .writeImage(4, &cInfo)
                .writeImage(5, &bakedInfo)
                .overwrite(engineState_->gbufferDescriptorSetRef(frameInfo.frameIndex));
        }

        // Use the "current-frame HZB" global descriptor set for the main scene after the HZB pass.
        auto const prevGlobalSet      = frameInfo.globalDescriptorSet;
        frameInfo.globalDescriptorSet = engineState_->renderContext->getGlobalDescriptorSetCurrentHzb(frameInfo.frameIndex);

        // Begin G-buffer with secondary command buffer support when model recording is multithreaded.
        renderer_.beginGbufferRenderPass(frameInfo.commandBuffer, true);
        engineState_->modelRenderSystem->renderGbuffer(frameInfo);
        renderer_.endOffscreenRenderPass(frameInfo.commandBuffer);
        renderer_.beginDeferredLightingRenderPass(frameInfo.commandBuffer);

        // Shadow descriptors update
        int const shadowCount     = engineState_->shadowSystem->getShadowLightCount();
        int const cubeShadowCount = engineState_->shadowSystem->getCubeShadowLightCount();

        std::array<VkDescriptorImageInfo, ShadowSystem::MAX_SHADOW_MAPS> shadowInfos{};
        for (int i = 0; i < shadowCount && i < ShadowSystem::MAX_SHADOW_MAPS; i++) {
            shadowInfos[i] = engineState_->shadowSystem->getShadowMapDescriptorInfo(i);
        }
        for (int i = shadowCount; i < ShadowSystem::MAX_SHADOW_MAPS; i++) {
            shadowInfos[i] = engineState_->shadowSystem->getShadowMapDescriptorInfo(0);
        }

        std::array<VkDescriptorImageInfo, ShadowSystem::MAX_CUBE_SHADOW_MAPS> cubeShadowInfos{};
        for (int i = 0; i < cubeShadowCount && i < ShadowSystem::MAX_CUBE_SHADOW_MAPS; i++) {
            cubeShadowInfos[i] = engineState_->shadowSystem->getCubeShadowMapDescriptorInfo(i);
        }
        for (int i = cubeShadowCount; i < ShadowSystem::MAX_CUBE_SHADOW_MAPS; i++) {
            cubeShadowInfos[i] = engineState_->shadowSystem->getCubeShadowMapDescriptorInfo(0);
        }

        std::array<VkWriteDescriptorSet, 2> descriptorWrites{};

        descriptorWrites[0].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[0].dstSet          = engineState_->deferredShadowDescriptorSetRef(frameInfo.frameIndex);
        descriptorWrites[0].dstBinding      = 0;
        descriptorWrites[0].dstArrayElement = 0;
        descriptorWrites[0].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrites[0].descriptorCount = ShadowSystem::MAX_SHADOW_MAPS;
        descriptorWrites[0].pImageInfo      = shadowInfos.data();

        descriptorWrites[1].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[1].dstSet          = engineState_->deferredShadowDescriptorSetRef(frameInfo.frameIndex);
        descriptorWrites[1].dstBinding      = 1;
        descriptorWrites[1].dstArrayElement = 0;
        descriptorWrites[1].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrites[1].descriptorCount = ShadowSystem::MAX_CUBE_SHADOW_MAPS;
        descriptorWrites[1].pImageInfo      = cubeShadowInfos.data();

        vkUpdateDescriptorSets(device_.device(), static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);

        engineState_->deferredLightingSystem->render(frameInfo,
            frameInfo.globalDescriptorSet,
            engineState_->getGbufferDescriptorSet(frameInfo.frameIndex),
            engineState_->getDeferredShadowDescriptorSet(frameInfo.frameIndex),
            engineState_->getDeferredIblDescriptorSet(frameInfo.frameIndex));
        renderer_.endOffscreenRenderPass(frameInfo.commandBuffer);

        if (debugMode_ == 0) {
            // Copy color -> scene color, then perform transmission/alpha+dust passes
            renderer_.copyOffscreenColorToSceneColor(frameInfo.commandBuffer);

            renderer_.beginOffscreenRenderPassLoadColorDepth(frameInfo.commandBuffer);
            engineState_->modelRenderSystem->renderTransmission(frameInfo);
            engineState_->modelRenderSystem->renderAlphaBlend(frameInfo);

            // Dust rendering uses the current scene + camera to compute lighting
            SunInfo const sunInfo = queryPrimaryDirectionalLightSunInfo(*frameInfo.scene);

            glm::vec3 sunColor     = glm::vec3(1.0f);
            glm::vec3 ambientColor = glm::vec3(0.1f);

            float const height = sunInfo.directionToSun.y;
            if (height > 0.1f) {
                sunColor     = glm::vec3(1.0f, 0.95f, 0.9f);
                ambientColor = glm::vec3(0.2f, 0.2f, 0.3f);
            } else if (height > -0.1f) {
                sunColor     = glm::vec3(1.0f, 0.6f, 0.3f);
                ambientColor = glm::vec3(0.3f, 0.2f, 0.2f);
            } else {
                sunColor     = glm::vec3(0.05f, 0.05f, 0.1f);
                ambientColor = glm::vec3(0.01f, 0.01f, 0.02f);
            }

            glm::vec4 const sunDirWithIntensity = glm::vec4(sunInfo.directionToSun, sunInfo.intensity);
            engineState_->dustRenderSystem->render(frameInfo, engineState_->dustSettings, sunDirWithIntensity, sunColor, ambientColor);

            renderer_.endOffscreenRenderPass(frameInfo.commandBuffer);
        }

        frameInfo.globalDescriptorSet = prevGlobalSet;

        renderer_.generateOffscreenMipmaps(frameInfo.commandBuffer);
    }

}  // namespace engine
