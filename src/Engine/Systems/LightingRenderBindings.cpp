#include "Engine/Systems/LightingRenderBindings.hpp"

#include <array>
#include <stdexcept>
#include <vector>

#include "Engine/Graphics/FrameInfo.hpp"
#include "Engine/Graphics/SwapChain.hpp"
#include "Engine/Systems/IBLSystem.hpp"
#include "Engine/Systems/ShadowSystem.hpp"
namespace engine {
    LightingRenderBindings::LightingRenderBindings(Device& device) : device_(device) {}
    LightingRenderBindings::~LightingRenderBindings() {
        if (shadowDescriptorPool_ != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(device_.device(), shadowDescriptorPool_, nullptr);
        }
        if (shadowDescriptorSetLayout_ != VK_NULL_HANDLE) {
            vkDestroyDescriptorSetLayout(device_.device(), shadowDescriptorSetLayout_, nullptr);
        }
        if (iblDescriptorPool_ != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(device_.device(), iblDescriptorPool_, nullptr);
        }
        if (iblDescriptorSetLayout_ != VK_NULL_HANDLE) {
            vkDestroyDescriptorSetLayout(device_.device(), iblDescriptorSetLayout_, nullptr);
        }
    }
    void LightingRenderBindings::createResources() {
        createShadowDescriptorResources();
        createIBLDescriptorResources();
    }
    void LightingRenderBindings::setShadowSystem(ShadowSystem* shadowSystem) {
        shadowSystem_ = shadowSystem;
    }
    void LightingRenderBindings::setIBLSystem(IBLSystem* iblSystem) {
        iblSystem_ = iblSystem;
    }
    void LightingRenderBindings::createShadowDescriptorResources() {
        std::array<VkDescriptorSetLayoutBinding, 2> bindings{};
        bindings[0].binding            = 0;
        bindings[0].descriptorType     = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        bindings[0].descriptorCount    = ShadowSystem::MAX_SHADOW_MAPS;
        bindings[0].stageFlags         = VK_SHADER_STAGE_FRAGMENT_BIT;
        bindings[0].pImmutableSamplers = nullptr;
        bindings[1].binding            = 1;
        bindings[1].descriptorType     = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        bindings[1].descriptorCount    = ShadowSystem::MAX_CUBE_SHADOW_MAPS;
        bindings[1].stageFlags         = VK_SHADER_STAGE_FRAGMENT_BIT;
        bindings[1].pImmutableSamplers = nullptr;
        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
        layoutInfo.pBindings    = bindings.data();
        if (vkCreateDescriptorSetLayout(device_.device(), &layoutInfo, nullptr, &shadowDescriptorSetLayout_) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create shadow descriptor set layout");
        }
        std::array<VkDescriptorPoolSize, 2> poolSizes{};
        poolSizes[0].type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        poolSizes[0].descriptorCount = static_cast<uint32_t>(SwapChain::maxFramesInFlight() * ShadowSystem::MAX_SHADOW_MAPS);
        poolSizes[1].type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        poolSizes[1].descriptorCount = static_cast<uint32_t>(SwapChain::maxFramesInFlight() * ShadowSystem::MAX_CUBE_SHADOW_MAPS);
        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
        poolInfo.pPoolSizes    = poolSizes.data();
        poolInfo.maxSets       = static_cast<uint32_t>(SwapChain::maxFramesInFlight());
        if (vkCreateDescriptorPool(device_.device(), &poolInfo, nullptr, &shadowDescriptorPool_) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create shadow descriptor pool");
        }
        std::vector<VkDescriptorSetLayout> layouts(SwapChain::maxFramesInFlight(), shadowDescriptorSetLayout_);
        VkDescriptorSetAllocateInfo        allocInfo{};
        allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool     = shadowDescriptorPool_;
        allocInfo.descriptorSetCount = static_cast<uint32_t>(SwapChain::maxFramesInFlight());
        allocInfo.pSetLayouts        = layouts.data();
        shadowDescriptorSets_.resize(SwapChain::maxFramesInFlight());
        if (vkAllocateDescriptorSets(device_.device(), &allocInfo, shadowDescriptorSets_.data()) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate shadow descriptor sets");
        }
    }
    void LightingRenderBindings::createIBLDescriptorResources() {
        std::array<VkDescriptorSetLayoutBinding, 3> bindings{};
        bindings[0].binding            = 0;
        bindings[0].descriptorType     = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        bindings[0].descriptorCount    = 1;
        bindings[0].stageFlags         = VK_SHADER_STAGE_FRAGMENT_BIT;
        bindings[0].pImmutableSamplers = nullptr;
        bindings[1].binding            = 1;
        bindings[1].descriptorType     = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        bindings[1].descriptorCount    = 1;
        bindings[1].stageFlags         = VK_SHADER_STAGE_FRAGMENT_BIT;
        bindings[1].pImmutableSamplers = nullptr;
        bindings[2].binding            = 2;
        bindings[2].descriptorType     = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        bindings[2].descriptorCount    = 1;
        bindings[2].stageFlags         = VK_SHADER_STAGE_FRAGMENT_BIT;
        bindings[2].pImmutableSamplers = nullptr;
        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
        layoutInfo.pBindings    = bindings.data();
        if (vkCreateDescriptorSetLayout(device_.device(), &layoutInfo, nullptr, &iblDescriptorSetLayout_) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create IBL descriptor set layout");
        }
        std::array<VkDescriptorPoolSize, 1> poolSizes{};
        poolSizes[0].type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        poolSizes[0].descriptorCount = static_cast<uint32_t>(SwapChain::maxFramesInFlight() * 3);
        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
        poolInfo.pPoolSizes    = poolSizes.data();
        poolInfo.maxSets       = static_cast<uint32_t>(SwapChain::maxFramesInFlight());
        if (vkCreateDescriptorPool(device_.device(), &poolInfo, nullptr, &iblDescriptorPool_) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create IBL descriptor pool");
        }
        std::vector<VkDescriptorSetLayout> layouts(SwapChain::maxFramesInFlight(), iblDescriptorSetLayout_);
        VkDescriptorSetAllocateInfo        allocInfo{};
        allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool     = iblDescriptorPool_;
        allocInfo.descriptorSetCount = static_cast<uint32_t>(SwapChain::maxFramesInFlight());
        allocInfo.pSetLayouts        = layouts.data();
        iblDescriptorSets_.resize(SwapChain::maxFramesInFlight());
        if (vkAllocateDescriptorSets(device_.device(), &allocInfo, iblDescriptorSets_.data()) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate IBL descriptor sets");
        }
    }
    void LightingRenderBindings::bindShadow(FrameInfo& frameInfo, VkPipelineLayout pipelineLayout) {
        if (shadowSystem_ == nullptr) {
            return;
        }
        if (shadowDescriptorSets_.empty() ||
            frameInfo.frameIndex < 0 || frameInfo.frameIndex >= static_cast<int>(shadowDescriptorSets_.size()) ||
            shadowDescriptorSets_[frameInfo.frameIndex] == VK_NULL_HANDLE) {
            return;
        }
        int const                                                        shadowCount     = shadowSystem_->getShadowLightCount();
        int const                                                        cubeShadowCount = shadowSystem_->getCubeShadowLightCount();
        std::array<VkDescriptorImageInfo, ShadowSystem::MAX_SHADOW_MAPS> shadowInfos{};
        for (int i = 0; i < shadowCount && i < ShadowSystem::MAX_SHADOW_MAPS; i++) {
            shadowInfos[i] = shadowSystem_->getShadowMapDescriptorInfo(i);
        }
        for (int i = shadowCount; i < ShadowSystem::MAX_SHADOW_MAPS; i++) {
            shadowInfos[i] = shadowSystem_->getShadowMapDescriptorInfo(0);
        }
        std::array<VkDescriptorImageInfo, ShadowSystem::MAX_CUBE_SHADOW_MAPS> cubeShadowInfos{};
        for (int i = 0; i < cubeShadowCount && i < ShadowSystem::MAX_CUBE_SHADOW_MAPS; i++) {
            cubeShadowInfos[i] = shadowSystem_->getCubeShadowMapDescriptorInfo(i);
        }
        for (int i = cubeShadowCount; i < ShadowSystem::MAX_CUBE_SHADOW_MAPS; i++) {
            cubeShadowInfos[i] = shadowSystem_->getCubeShadowMapDescriptorInfo(0);
        }
        std::array<VkWriteDescriptorSet, 2> descriptorWrites{};
        descriptorWrites[0].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[0].dstSet          = shadowDescriptorSets_[frameInfo.frameIndex];
        descriptorWrites[0].dstBinding      = 0;
        descriptorWrites[0].dstArrayElement = 0;
        descriptorWrites[0].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrites[0].descriptorCount = ShadowSystem::MAX_SHADOW_MAPS;
        descriptorWrites[0].pImageInfo      = shadowInfos.data();
        descriptorWrites[1].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[1].dstSet          = shadowDescriptorSets_[frameInfo.frameIndex];
        descriptorWrites[1].dstBinding      = 1;
        descriptorWrites[1].dstArrayElement = 0;
        descriptorWrites[1].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrites[1].descriptorCount = ShadowSystem::MAX_CUBE_SHADOW_MAPS;
        descriptorWrites[1].pImageInfo      = cubeShadowInfos.data();
        vkUpdateDescriptorSets(device_.device(), static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
        assert(shadowDescriptorSets_[frameInfo.frameIndex] != VK_NULL_HANDLE && "LightingRenderBindings: shadow descriptor set is null");
        vkCmdBindDescriptorSets(frameInfo.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, kShadowSetIndex, 1, &shadowDescriptorSets_[frameInfo.frameIndex], 0, nullptr);
    }
    void LightingRenderBindings::bindIBL(FrameInfo& frameInfo, VkPipelineLayout pipelineLayout) {
        if (iblSystem_ == nullptr) {
            return;
        }
        if (iblDescriptorSets_.empty() ||
            frameInfo.frameIndex < 0 || frameInfo.frameIndex >= static_cast<int>(iblDescriptorSets_.size()) ||
            iblDescriptorSets_[frameInfo.frameIndex] == VK_NULL_HANDLE) {
            return;
        }
        VkDescriptorImageInfo const         irradianceInfo = iblSystem_->getIrradianceDescriptorInfo();
        VkDescriptorImageInfo const         prefilterInfo  = iblSystem_->getPrefilteredDescriptorInfo();
        VkDescriptorImageInfo const         brdfInfo       = iblSystem_->getBRDFLUTDescriptorInfo();
        std::array<VkWriteDescriptorSet, 3> descriptorWrites{};
        descriptorWrites[0].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[0].dstSet          = iblDescriptorSets_[frameInfo.frameIndex];
        descriptorWrites[0].dstBinding      = 0;
        descriptorWrites[0].dstArrayElement = 0;
        descriptorWrites[0].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrites[0].descriptorCount = 1;
        descriptorWrites[0].pImageInfo      = &irradianceInfo;
        descriptorWrites[1].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[1].dstSet          = iblDescriptorSets_[frameInfo.frameIndex];
        descriptorWrites[1].dstBinding      = 1;
        descriptorWrites[1].dstArrayElement = 0;
        descriptorWrites[1].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrites[1].descriptorCount = 1;
        descriptorWrites[1].pImageInfo      = &prefilterInfo;
        descriptorWrites[2].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[2].dstSet          = iblDescriptorSets_[frameInfo.frameIndex];
        descriptorWrites[2].dstBinding      = 2;
        descriptorWrites[2].dstArrayElement = 0;
        descriptorWrites[2].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrites[2].descriptorCount = 1;
        descriptorWrites[2].pImageInfo      = &brdfInfo;
        vkUpdateDescriptorSets(device_.device(), static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
        vkCmdBindDescriptorSets(frameInfo.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, kIBLSetIndex, 1, &iblDescriptorSets_[frameInfo.frameIndex], 0, nullptr);
    }
}  // namespace engine
