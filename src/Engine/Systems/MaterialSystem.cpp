#include "Engine/Systems/MaterialSystem.hpp"

#include <stdexcept>

#include "Engine/Graphics/Descriptors.hpp"
#include "Engine/Graphics/Device.hpp"

#include "ModelLib/Resources/PBRMaterial.hpp"
#include "ModelLib/Resources/Texture.hpp"
#include "vulkan/vulkan_core.h"
namespace engine {
    MaterialSystem::MaterialSystem(Device& device) : device_(device) {
        createDefaultTextures();
        createMaterialDescriptorSetLayout();
        createMaterialDescriptorPool();
    }
    void MaterialSystem::createDefaultTextures() {
        defaultWhiteTexture_  = Texture::createWhiteTexture(device_);
        defaultNormalTexture_ = Texture::createNormalTexture(device_);
    }
    void MaterialSystem::createMaterialDescriptorSetLayout() {
        materialSetLayout_ = DescriptorSetLayout::Builder(device_)
                                 .addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
                                 .addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
                                 .addBinding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
                                 .addBinding(3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
                                 .addBinding(4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
                                 .build();
    }
    void MaterialSystem::createMaterialDescriptorPool() {
        materialDescriptorPool_ = DescriptorPool::Builder(device_)
                                      .setMaxSets(1000)
                                      .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 5000)
                                      .build();
    }
    VkDescriptorSet MaterialSystem::getMaterialDescriptorSet(const PBRMaterial& material) {
        auto const materialHash = reinterpret_cast<size_t>(&material);
        auto       it           = materialDescriptorCache_.find(materialHash);
        if (it != materialDescriptorCache_.end()) {
            return it->second;
        }
        VkDescriptorSet descriptorSet;
        if (!materialDescriptorPool_->allocateDescriptor(materialSetLayout_->getDescriptorSetLayout(), descriptorSet)) {
            throw std::runtime_error("Failed to allocate material descriptor set!");
        }
        DescriptorWriter      writer(*materialSetLayout_, *materialDescriptorPool_);
        VkDescriptorImageInfo albedoInfo = material.albedoMap ? material.albedoMap->getDescriptorInfo() : defaultWhiteTexture_->getDescriptorInfo();
        writer.writeImage(0, &albedoInfo);
        VkDescriptorImageInfo normalInfo = material.normalMap ? material.normalMap->getDescriptorInfo() : defaultNormalTexture_->getDescriptorInfo();
        writer.writeImage(1, &normalInfo);
        VkDescriptorImageInfo metallicInfo = material.metallicMap ? material.metallicMap->getDescriptorInfo() : defaultWhiteTexture_->getDescriptorInfo();
        writer.writeImage(2, &metallicInfo);
        VkDescriptorImageInfo roughnessInfo = material.roughnessMap ? material.roughnessMap->getDescriptorInfo() : defaultWhiteTexture_->getDescriptorInfo();
        writer.writeImage(3, &roughnessInfo);
        VkDescriptorImageInfo aoInfo = material.aoMap ? material.aoMap->getDescriptorInfo() : defaultWhiteTexture_->getDescriptorInfo();
        writer.writeImage(4, &aoInfo);
        writer.overwrite(descriptorSet);
        materialDescriptorCache_[materialHash] = descriptorSet;
        return descriptorSet;
    }
}  // namespace engine
