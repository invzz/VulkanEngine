#include "3dEngine/systems/MaterialSystem.hpp"

#include <stdexcept>

#include "3dEngine/Texture.hpp"

namespace engine {

  MaterialSystem::MaterialSystem(Device& device) : device_(device)
  {
    createDefaultTextures();
    createMaterialDescriptorSetLayout();
    createMaterialDescriptorPool();
  }

  void MaterialSystem::createDefaultTextures()
  {
    defaultWhiteTexture_  = Texture::createWhiteTexture(device_);
    defaultNormalTexture_ = Texture::createNormalTexture(device_);
  }

  void MaterialSystem::createMaterialDescriptorSetLayout()
  {
    materialSetLayout_ = DescriptorSetLayout::Builder(device_)
                                 .addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT) // albedo
                                 .addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT) // normal
                                 .addBinding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT) // metallic
                                 .addBinding(3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT) // roughness
                                 .addBinding(4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT) // ao
                                 .build();
  }

  void MaterialSystem::createMaterialDescriptorPool()
  {
    materialDescriptorPool_ = DescriptorPool::Builder(device_)
                                      .setMaxSets(1000) // Support many materials
                                      .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 5000)
                                      .build();
  }

  VkDescriptorSet MaterialSystem::getMaterialDescriptorSet(const PBRMaterial& material)
  {
    // Use material pointer as hash key
    size_t materialHash = reinterpret_cast<size_t>(&material);

    // Check cache first
    auto it = materialDescriptorCache_.find(materialHash);
    if (it != materialDescriptorCache_.end())
    {
      return it->second;
    }

    // Create new descriptor set
    VkDescriptorSet descriptorSet;
    if (!materialDescriptorPool_->allocateDescriptor(materialSetLayout_->getDescriptorSetLayout(), descriptorSet))
    {
      throw std::runtime_error("Failed to allocate material descriptor set!");
    }

    // Write descriptor set with textures or default fallbacks
    DescriptorWriter writer(*materialSetLayout_, *materialDescriptorPool_);

    // Get descriptor infos into local variables (getDescriptorInfo returns by value)
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

    // Cache the descriptor set
    materialDescriptorCache_[materialHash] = descriptorSet;

    return descriptorSet;
  }

} // namespace engine
