#include "Engine/Graphics/DescriptorManager.hpp"

#include <cassert>
#include <stdexcept>

#include "Engine/EngineState.hpp"
#include "Engine/Graphics/Device.hpp"
#include "Engine/Graphics/Pipeline.hpp"
#include "Engine/Graphics/SwapChain.hpp"
#include "Engine/Systems/IBLSystem.hpp"
#include "Engine/Systems/ShadowSystem.hpp"

namespace engine {

    void DescriptorManager::createDescriptorResources(Device& device, Renderer& renderer) {
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

        deferredIblPool_ = DescriptorPool::Builder(device)
                               .setMaxSets(SwapChain::maxFramesInFlight() * 4)
                               .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, SwapChain::maxFramesInFlight() * 8)
                               .build();

        deferredIblSetLayout_ = DescriptorSetLayout::Builder(device)
                                    .addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
                                    .addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
                                    .addBinding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
                                    .build();

        deferredShadowPool_ = DescriptorPool::Builder(device)
                                  .setMaxSets(SwapChain::maxFramesInFlight() * 4)
                                  .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, SwapChain::maxFramesInFlight() * 16)
                                  .build();

        deferredShadowSetLayout_ = DescriptorSetLayout::Builder(device)
                                       .addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, ShadowSystem::MAX_SHADOW_MAPS)
                                       .addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, ShadowSystem::MAX_CUBE_SHADOW_MAPS)
                                       .build();

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

        deferredIblDescriptorSets_.resize(SwapChain::maxFramesInFlight());
        for (int i = 0; i < static_cast<int>(deferredIblDescriptorSets_.size()); ++i) {
            deferredIblDescriptorSets_[i] = VK_NULL_HANDLE;
        }

        deferredShadowDescriptorSets_.resize(SwapChain::maxFramesInFlight());
        for (int i = 0; i < static_cast<int>(deferredShadowDescriptorSets_.size()); ++i) {
            if (!deferredShadowPool_->allocateDescriptor(deferredShadowSetLayout_->getDescriptorSetLayout(), deferredShadowDescriptorSets_[i])) {
                throw std::runtime_error("Failed to allocate deferred shadow descriptor set");
            }
        }

        postProcessDescriptorSets_.resize(SwapChain::maxFramesInFlight());
        for (int i = 0; i < static_cast<int>(postProcessDescriptorSets_.size()); ++i) {
            auto imageInfo = renderer.getOffscreenImageInfo(i);
            auto depthInfo = renderer.getDepthImageInfo(i);
            DescriptorWriter(*postProcessSetLayout_, *postProcessPool_)
                .writeImage(0, &imageInfo)
                .writeImage(1, &depthInfo)
                .buildOrThrow(postProcessDescriptorSets_[i]);
        }

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

    void DescriptorManager::updatePostProcessDescriptors(int frameIndex, Renderer& renderer) {
        if (frameIndex < 0 || frameIndex >= static_cast<int>(postProcessDescriptorSets_.size()) || postProcessDescriptorSets_[frameIndex] == VK_NULL_HANDLE) {
            return;
        }

        auto imageInfo = renderer.getOffscreenImageInfo(frameIndex);
        auto depthInfo = renderer.getDepthImageInfo(frameIndex);

        DescriptorWriter(*postProcessSetLayout_, *postProcessPool_)
            .writeImage(0, &imageInfo)
            .writeImage(1, &depthInfo)
            .overwrite(postProcessDescriptorSets_[frameIndex]);
    }

    void DescriptorManager::recreatePostProcessDescriptorSets(Device& device, Renderer& renderer, VkDescriptorSetLayout existingLayout) {
        postProcessSetLayout_ = DescriptorSetLayout::Builder(device)
                                    .addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
                                    .addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
                                    .build();

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
