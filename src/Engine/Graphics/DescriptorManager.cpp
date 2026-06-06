#include "Engine/Graphics/DescriptorManager.hpp"

#include <cassert>
#include <stdexcept>

#include "Engine/EngineState.hpp"
#include "Engine/Graphics/Device.hpp"
#include "Engine/Graphics/Pipeline.hpp"
#include "Engine/Graphics/SwapChain.hpp"
#include "Engine/Systems/DeferredLightingSystem.hpp"
#include "Engine/Systems/IBLSystem.hpp"
#include "Engine/Systems/ModelRenderSystem.hpp"
#include "Engine/Systems/PostProcessingSystem.hpp"
#include "Engine/Systems/ShadowSystem.hpp"

namespace engine {

    void DescriptorManager::createDescriptorResources(Device& device, Renderer& renderer) {
        // G-buffer pool and layout
        // G-buffer pool and layout
        gbufferPool_ = DescriptorPool::Builder(device)
                           .setMaxSets(SwapChain::maxFramesInFlight() * 4)
                           .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, SwapChain::maxFramesInFlight() * 8)
                           .build();

        gbufferSetLayout_ = DescriptorSetLayout::Builder(device)
                                .addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
                                .addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
                                .addBinding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
                                .addBinding(3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
                                .build();

        // Deferred IBL pool and layout
        deferredIblPool_ = DescriptorPool::Builder(device)
                               .setMaxSets(SwapChain::maxFramesInFlight() * 4)
                               .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, SwapChain::maxFramesInFlight() * 8)
                               .build();

        deferredIblSetLayout_ = DescriptorSetLayout::Builder(device)
                                    .addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
                                    .addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
                                    .addBinding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
                                    .build();

        // Deferred Shadow pool and layout
        deferredShadowPool_ = DescriptorPool::Builder(device)
                                  .setMaxSets(SwapChain::maxFramesInFlight() * 4)
                                  .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, SwapChain::maxFramesInFlight() * 16)
                                  .build();

        deferredShadowSetLayout_ = DescriptorSetLayout::Builder(device)
                                       .addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, ShadowSystem::MAX_SHADOW_MAPS)
                                       .addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, ShadowSystem::MAX_CUBE_SHADOW_MAPS)
                                       .build();

        // Post-processing pool and layout
        postProcessPool_ = DescriptorPool::Builder(device)
                               .setMaxSets(SwapChain::maxFramesInFlight() * 4)
                               .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, SwapChain::maxFramesInFlight() * 8)
                               .build();

        postProcessSetLayout_ = DescriptorSetLayout::Builder(device)
                                    .addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
                                    .addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
                                    .build();
    }

    void DescriptorManager::allocatePerFrameDescriptors(Renderer& renderer) {
        // G-buffer per-frame descriptor sets
        gbufferDescriptorSets_.resize(SwapChain::maxFramesInFlight());
        for (int i = 0; i < static_cast<int>(gbufferDescriptorSets_.size()); ++i) {
            auto nInfo = renderer.getGbufferNormalImageInfo(i);
            auto aInfo = renderer.getGbufferAlbedoImageInfo(i);
            auto mInfo = renderer.getGbufferMaterialImageInfo(i);
            auto dInfo = renderer.getDepthImageInfo(i);
            DescriptorWriter(*gbufferSetLayout_, *gbufferPool_)
                .writeImage(0, &nInfo)
                .writeImage(1, &aInfo)
                .writeImage(2, &mInfo)
                .writeImage(3, &dInfo)
                .buildOrThrow(gbufferDescriptorSets_[i]);
        }

        // Deferred IBL per-frame descriptor sets
        deferredIblDescriptorSets_.resize(SwapChain::maxFramesInFlight());
        for (int i = 0; i < static_cast<int>(deferredIblDescriptorSets_.size()); ++i) {
            // IBL descriptor sets are allocated in initPostProcessing via EngineState
            // because they need IBLSystem access. Initialize with null handles.
            deferredIblDescriptorSets_[i] = VK_NULL_HANDLE;
        }

        // Deferred Shadow per-frame descriptor sets
        deferredShadowDescriptorSets_.resize(SwapChain::maxFramesInFlight());
        for (int i = 0; i < static_cast<int>(deferredShadowDescriptorSets_.size()); ++i) {
            if (!deferredShadowPool_->allocateDescriptor(deferredShadowSetLayout_->getDescriptorSetLayout(), deferredShadowDescriptorSets_[i])) {
                throw std::runtime_error("Failed to allocate deferred shadow descriptor set");
            }
        }

        // Post-processing per-frame descriptor sets
        postProcessDescriptorSets_.resize(SwapChain::maxFramesInFlight());
        for (int i = 0; i < static_cast<int>(postProcessDescriptorSets_.size()); ++i) {
            auto imageInfo = renderer.getOffscreenImageInfo(i);
            auto depthInfo = renderer.getDepthImageInfo(i);
            DescriptorWriter(*postProcessSetLayout_, *postProcessPool_)
                .writeImage(0, &imageInfo)
                .writeImage(1, &depthInfo)
                .buildOrThrow(postProcessDescriptorSets_[i]);
        }

        // Global validation: ensure no null handles slipped through
        for (const auto& ds : gbufferDescriptorSets_)
            assert(ds != VK_NULL_HANDLE);
        for (const auto& ds : postProcessDescriptorSets_)
            assert(ds != VK_NULL_HANDLE);
        for (const auto& ds : deferredShadowDescriptorSets_)
            assert(ds != VK_NULL_HANDLE);
    }

    VkDescriptorSet DescriptorManager::gbufferDescriptorSet(int frameIndex) const {
        if (frameIndex < 0 || frameIndex >= static_cast<int>(gbufferDescriptorSets_.size())) {
            return VK_NULL_HANDLE;
        }
        return gbufferDescriptorSets_[frameIndex];
    }

    VkDescriptorSet& DescriptorManager::gbufferDescriptorSetRef(int frameIndex) {
        return gbufferDescriptorSets_.at(static_cast<size_t>(frameIndex));
    }

    VkDescriptorSet DescriptorManager::deferredIblDescriptorSet(int frameIndex) const {
        if (frameIndex < 0 || frameIndex >= static_cast<int>(deferredIblDescriptorSets_.size())) {
            return VK_NULL_HANDLE;
        }
        return deferredIblDescriptorSets_[frameIndex];
    }

    std::vector<VkDescriptorSet>& DescriptorManager::deferredIblDescriptorSets() {
        return deferredIblDescriptorSets_;
    }

    VkDescriptorSet DescriptorManager::deferredShadowDescriptorSet(int frameIndex) const {
        if (frameIndex < 0 || frameIndex >= static_cast<int>(deferredShadowDescriptorSets_.size())) {
            return VK_NULL_HANDLE;
        }
        return deferredShadowDescriptorSets_[frameIndex];
    }

    VkDescriptorSet& DescriptorManager::deferredShadowDescriptorSetRef(int frameIndex) {
        return deferredShadowDescriptorSets_.at(static_cast<size_t>(frameIndex));
    }

    VkDescriptorSet DescriptorManager::postProcessDescriptorSet(int frameIndex) const {
        if (frameIndex < 0 || frameIndex >= static_cast<int>(postProcessDescriptorSets_.size())) {
            return VK_NULL_HANDLE;
        }
        return postProcessDescriptorSets_[frameIndex];
    }

    VkDescriptorSet& DescriptorManager::postProcessDescriptorSetRef(int frameIndex) {
        return postProcessDescriptorSets_.at(static_cast<size_t>(frameIndex));
    }

    void DescriptorManager::updateGbufferDescriptors(int frameIndex, Renderer& renderer) {
        auto nInfo = renderer.getGbufferNormalImageInfo(frameIndex);
        auto aInfo = renderer.getGbufferAlbedoImageInfo(frameIndex);
        auto mInfo = renderer.getGbufferMaterialImageInfo(frameIndex);
        auto dInfo = renderer.getDepthImageInfo(frameIndex);

        DescriptorWriter(*gbufferSetLayout_, *gbufferPool_)
            .writeImage(0, &nInfo)
            .writeImage(1, &aInfo)
            .writeImage(2, &mInfo)
            .writeImage(3, &dInfo)
            .overwrite(gbufferDescriptorSets_[frameIndex]);
    }

    void DescriptorManager::updateShadowDescriptors(int frameIndex, ShadowSystem& shadowSystem, Device& device) {
        if (frameIndex < 0 || frameIndex >= static_cast<int>(deferredShadowDescriptorSets_.size()) || deferredShadowDescriptorSets_[frameIndex] == VK_NULL_HANDLE) {
            return;
        }
        int const shadowCount     = shadowSystem.getShadowLightCount();
        int const cubeShadowCount = shadowSystem.getCubeShadowLightCount();

        std::array<VkDescriptorImageInfo, ShadowSystem::MAX_SHADOW_MAPS> shadowInfos{};
        for (int i = 0; i < shadowCount && i < ShadowSystem::MAX_SHADOW_MAPS; i++) {
            shadowInfos[i] = shadowSystem.getShadowMapDescriptorInfo(i);
        }
        for (int i = shadowCount; i < ShadowSystem::MAX_SHADOW_MAPS; i++) {
            shadowInfos[i] = shadowSystem.getShadowMapDescriptorInfo(0);
        }

        std::array<VkDescriptorImageInfo, ShadowSystem::MAX_CUBE_SHADOW_MAPS> cubeShadowInfos{};
        for (int i = 0; i < cubeShadowCount && i < ShadowSystem::MAX_CUBE_SHADOW_MAPS; i++) {
            cubeShadowInfos[i] = shadowSystem.getCubeShadowMapDescriptorInfo(i);
        }
        for (int i = cubeShadowCount; i < ShadowSystem::MAX_CUBE_SHADOW_MAPS; i++) {
            cubeShadowInfos[i] = shadowSystem.getCubeShadowMapDescriptorInfo(0);
        }

        VkDescriptorSet shadowDescriptorSet = deferredShadowDescriptorSets_[frameIndex];

        std::array<VkWriteDescriptorSet, 2> descriptorWrites{};

        descriptorWrites[0].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[0].dstSet          = shadowDescriptorSet;
        descriptorWrites[0].dstBinding      = 0;
        descriptorWrites[0].dstArrayElement = 0;
        descriptorWrites[0].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrites[0].descriptorCount = ShadowSystem::MAX_SHADOW_MAPS;
        descriptorWrites[0].pImageInfo      = shadowInfos.data();

        descriptorWrites[1].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[1].dstSet          = shadowDescriptorSet;
        descriptorWrites[1].dstBinding      = 1;
        descriptorWrites[1].dstArrayElement = 0;
        descriptorWrites[1].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrites[1].descriptorCount = ShadowSystem::MAX_CUBE_SHADOW_MAPS;
        descriptorWrites[1].pImageInfo      = cubeShadowInfos.data();

        vkUpdateDescriptorSets(device.device(), static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
    }

    void DescriptorManager::recreatePostProcessDescriptorSets(Device& device, Renderer& renderer, VkDescriptorSetLayout existingLayout) {
        // Update the existing layout
        postProcessSetLayout_ = DescriptorSetLayout::Builder(device)
                                    .addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
                                    .addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
                                    .build();

        // Recreate descriptor sets
        postProcessDescriptorSets_.resize(SwapChain::maxFramesInFlight());
        for (int i = 0; i < static_cast<int>(postProcessDescriptorSets_.size()); ++i) {
            auto imageInfo = renderer.getOffscreenImageInfo(i);
            auto depthInfo = renderer.getDepthImageInfo(i);
            DescriptorWriter(*postProcessSetLayout_, *postProcessPool_)
                .writeImage(0, &imageInfo)
                .writeImage(1, &depthInfo)
                .build(postProcessDescriptorSets_[i]);
        }
    }

}  // namespace engine
