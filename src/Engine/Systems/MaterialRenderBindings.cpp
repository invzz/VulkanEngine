#include "Engine/Systems/MaterialRenderBindings.hpp"

#include <cassert>
#include <cstring>
#include <stdexcept>

#include "Engine/Graphics/FrameInfo.hpp"
#include "Engine/Graphics/SwapChain.hpp"
#include "Engine/Resources/MaterialUniformData.hpp"
#include "Engine/Resources/PBRMaterial.hpp"
#include "Engine/Resources/Texture.hpp"

namespace engine {
  namespace {
    constexpr float kFeatureEpsCpu = 0.01f;
  }

  MaterialRenderBindings::MaterialRenderBindings(Device& device) : device_(device) {}

  MaterialRenderBindings::~MaterialRenderBindings()
  {
    if (descriptorPool_ != VK_NULL_HANDLE)
    {
      vkDestroyDescriptorPool(device_.device(), descriptorPool_, nullptr);
    }
    if (descriptorSetLayout_ != VK_NULL_HANDLE)
    {
      vkDestroyDescriptorSetLayout(device_.device(), descriptorSetLayout_, nullptr);
    }
  }

  void MaterialRenderBindings::createResources()
  {
    createDescriptorSetLayout();

    atomSize_ = materialAtomSize();

    createBuffers();
    createPoolAndSets();
    updateDescriptorSets();

    dynamicOffsetIndexByFrame_.resize(SwapChain::maxFramesInFlight(), 0u);
  }

  void MaterialRenderBindings::beginFrame(int frameIndex)
  {
    if (frameIndex < 0) return;
    if (dynamicOffsetIndexByFrame_.empty())
    {
      dynamicOffsetIndexByFrame_.resize(SwapChain::maxFramesInFlight(), 0u);
    }
    if (frameIndex < static_cast<int>(dynamicOffsetIndexByFrame_.size()))
    {
      dynamicOffsetIndexByFrame_[frameIndex] = 0u;
    }
  }

  VkDeviceSize MaterialRenderBindings::materialAtomSize() const
  {
    VkDeviceSize const minAlignment = device_.getProperties().limits.minUniformBufferOffsetAlignment;
    VkDeviceSize       atomSize     = sizeof(MaterialUniformData);
    if (minAlignment > 0)
    {
      atomSize = (atomSize + minAlignment - 1) & ~(minAlignment - 1);
    }
    return atomSize;
  }

  void MaterialRenderBindings::createDescriptorSetLayout()
  {
    VkDescriptorSetLayoutBinding binding{};
    binding.binding            = 0;
    binding.descriptorType     = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    binding.descriptorCount    = 1;
    binding.stageFlags         = VK_SHADER_STAGE_FRAGMENT_BIT;
    binding.pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings    = &binding;

    if (vkCreateDescriptorSetLayout(device_.device(), &layoutInfo, nullptr, &descriptorSetLayout_) != VK_SUCCESS)
    {
      throw std::runtime_error("Failed to create material descriptor set layout");
    }
  }

  void MaterialRenderBindings::createBuffers()
  {
    buffers_.resize(SwapChain::maxFramesInFlight());
    for (size_t i = 0; std::cmp_less(i, SwapChain::maxFramesInFlight()); i++)
    {
      buffers_[i] =
              std::make_unique<Buffer>(device_, atomSize_, kMaxMaterialRecordsPerFrame, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
      buffers_[i]->map();
    }
  }

  void MaterialRenderBindings::createPoolAndSets()
  {
    VkDescriptorPoolSize poolSize{};
    poolSize.type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    poolSize.descriptorCount = static_cast<uint32_t>(SwapChain::maxFramesInFlight());

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes    = &poolSize;
    poolInfo.maxSets       = static_cast<uint32_t>(SwapChain::maxFramesInFlight());

    if (vkCreateDescriptorPool(device_.device(), &poolInfo, nullptr, &descriptorPool_) != VK_SUCCESS)
    {
      throw std::runtime_error("Failed to create material descriptor pool");
    }

    std::vector<VkDescriptorSetLayout> layouts(SwapChain::maxFramesInFlight(), descriptorSetLayout_);

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool     = descriptorPool_;
    allocInfo.descriptorSetCount = static_cast<uint32_t>(SwapChain::maxFramesInFlight());
    allocInfo.pSetLayouts        = layouts.data();

    descriptorSets_.resize(SwapChain::maxFramesInFlight());
    if (vkAllocateDescriptorSets(device_.device(), &allocInfo, descriptorSets_.data()) != VK_SUCCESS)
    {
      throw std::runtime_error("Failed to allocate material descriptor sets");
    }
  }

  void MaterialRenderBindings::updateDescriptorSets()
  {
    for (size_t i = 0; std::cmp_less(i, SwapChain::maxFramesInFlight()); i++)
    {
      VkDescriptorBufferInfo const bufferInfo = buffers_[i]->descriptorInfoForIndex(0);

      VkWriteDescriptorSet descriptorWrite{};
      descriptorWrite.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      descriptorWrite.dstSet          = descriptorSets_[i];
      descriptorWrite.dstBinding      = 0;
      descriptorWrite.dstArrayElement = 0;
      descriptorWrite.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
      descriptorWrite.descriptorCount = 1;
      descriptorWrite.pBufferInfo     = &bufferInfo;

      vkUpdateDescriptorSets(device_.device(), 1, &descriptorWrite, 0, nullptr);
    }
  }

  namespace {
    MaterialUniformData buildMaterialUniformData(const PBRMaterial* pMaterial, float isSelected)
  {
    MaterialUniformData matData{};

    if (pMaterial != nullptr)
    {
      const auto& material = *pMaterial;

      uint32_t textureFlags            = 0;
      uint32_t albedoIndex             = 0;
      uint32_t normalIndex             = 0;
      uint32_t metallicIndex           = 0;
      uint32_t roughnessIndex          = 0;
      uint32_t aoIndex                 = 0;
      uint32_t emissiveIndex           = 0;
      uint32_t specularGlossinessIndex = 0;
      uint32_t transmissionIndex       = 0;
      uint32_t clearcoatIndex          = 0;
      uint32_t clearcoatRoughnessIndex = 0;
      uint32_t clearcoatNormalIndex    = 0;

      if (material.hasAlbedoMap())
      {
        textureFlags |= (1 << 0);
        albedoIndex = material.albedoMap->getGlobalIndex();
      }
      if (material.hasNormalMap())
      {
        textureFlags |= (1 << 1);
        normalIndex = material.normalMap->getGlobalIndex();
      }
      if (material.hasMetallicMap())
      {
        textureFlags |= (1 << 2);
        metallicIndex = material.metallicMap->getGlobalIndex();
      }
      if (material.hasRoughnessMap())
      {
        textureFlags |= (1 << 3);
        roughnessIndex = material.roughnessMap->getGlobalIndex();
      }
      if (material.hasAOMap())
      {
        textureFlags |= (1 << 4);
        aoIndex = material.aoMap->getGlobalIndex();
      }
      if (material.hasEmissiveMap())
      {
        textureFlags |= (1 << 5);
        emissiveIndex = material.emissiveMap->getGlobalIndex();
      }

      if (material.specularGlossinessMap)
      {
        textureFlags |= (1 << 8);
        specularGlossinessIndex = material.specularGlossinessMap->getGlobalIndex();
      }

      if (material.hasTransmissionMap())
      {
        textureFlags |= (1 << 9);
        transmissionIndex = material.transmissionMap->getGlobalIndex();
      }
      if (material.hasClearcoatMap())
      {
        textureFlags |= (1 << 10);
        clearcoatIndex = material.clearcoatMap->getGlobalIndex();
      }
      if (material.hasClearcoatRoughnessMap())
      {
        textureFlags |= (1 << 11);
        clearcoatRoughnessIndex = material.clearcoatRoughnessMap->getGlobalIndex();
      }
      if (material.hasClearcoatNormalMap())
      {
        textureFlags |= (1 << 12);
        clearcoatNormalIndex = material.clearcoatNormalMap->getGlobalIndex();
      }

      if (material.useMetallicRoughnessTexture)
      {
        textureFlags |= (1 << 6);
      }
      if (material.useOcclusionRoughnessMetallicTexture)
      {
        textureFlags |= (1 << 7);
      }

      matData.albedo                   = material.albedo;
      matData.emissiveInfo             = glm::vec4(material.emissiveColor, material.emissiveStrength);
      matData.specularGlossinessFactor = glm::vec4(material.specularFactor, material.glossinessFactor);
      matData.attenuationColorAndDist  = glm::vec4(material.attenuationColor, material.attenuationDistance);

      // Col 0
      matData.params[0][0] = material.metallic;
      matData.params[0][1] = material.roughness;
      matData.params[0][2] = material.ao;
      matData.params[0][3] = isSelected;
      // Col 1
      matData.params[1][0] = material.clearcoat;
      matData.params[1][1] = material.clearcoatRoughness;
      matData.params[1][2] = material.anisotropic;
      matData.params[1][3] = material.anisotropicRotation;
      // Col 2
      matData.params[2][0] = material.transmission;
      matData.params[2][1] = material.ior;
      matData.params[2][2] = material.iridescence;
      matData.params[2][3] = material.iridescenceIOR;
      // Col 3
      matData.params[3][0] = material.iridescenceThickness;
      matData.params[3][1] = material.uvScale;
      matData.params[3][2] = material.alphaCutoff;
      matData.params[3][3] = material.thickness;

      matData.flagsAndIndices0.x = textureFlags;
      matData.flagsAndIndices0.y = static_cast<uint32_t>(material.alphaMode);
      matData.flagsAndIndices0.z = albedoIndex;
      matData.flagsAndIndices0.w = normalIndex;

      matData.indices1.x = metallicIndex;
      matData.indices1.y = roughnessIndex;
      matData.indices1.z = aoIndex;
      matData.indices1.w = emissiveIndex;

      matData.indices2.x = specularGlossinessIndex;
      matData.indices2.y = material.useSpecularGlossinessWorkflow ? 1 : 0;
      matData.indices2.z = transmissionIndex;
      matData.indices2.w = clearcoatIndex;

      matData.indices3.x = clearcoatRoughnessIndex;
      matData.indices3.y = clearcoatNormalIndex;
    }
    else
    {
      matData.albedo                   = glm::vec4(1.0f);
      matData.emissiveInfo             = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
      matData.specularGlossinessFactor = glm::vec4(1.0f);
      matData.attenuationColorAndDist  = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);

      matData.params[0][0] = 0.0f; // metallic
      matData.params[0][1] = 0.5f; // roughness
      matData.params[0][2] = 1.0f; // ao
      matData.params[0][3] = isSelected;

      matData.params[1][0] = 0.0f;  // clearcoat
      matData.params[1][1] = 0.03f; // clearcoatRoughness
      matData.params[1][2] = 0.0f;  // anisotropic
      matData.params[1][3] = 0.0f;  // anisotropicRotation

      matData.params[2][0] = 0.0f; // transmission
      matData.params[2][1] = 1.5f; // ior
      matData.params[2][2] = 0.0f; // iridescence
      matData.params[2][3] = 1.3f; // iridescenceIOR

      matData.params[3][0] = 100.0f; // iridescenceThickness
      matData.params[3][1] = 1.0f;   // uvScale
      matData.params[3][2] = 0.5f;   // alphaCutoff
      matData.params[3][3] = 0.0f;   // thickness

      matData.flagsAndIndices0 = glm::uvec4(0);
      matData.indices1         = glm::uvec4(0);
      matData.indices2         = glm::uvec4(0);
    }

    return matData;
  }

  } // namespace

  void MaterialRenderBindings::writeAndBind(FrameInfo& frameInfo, VkPipelineLayout pipelineLayout, const void* data, VkDeviceSize dataSize)
  {
    if (frameInfo.frameIndex < 0 || frameInfo.frameIndex >= static_cast<int>(buffers_.size()))
    {
      return;
    }

    uint32_t& dynamicOffsetIndex = dynamicOffsetIndexByFrame_[frameInfo.frameIndex];
    if (dynamicOffsetIndex >= kMaxMaterialRecordsPerFrame)
    {
      return;
    }

    char* mappedData = reinterpret_cast<char*>(buffers_[frameInfo.frameIndex]->getMappedMemory());
    std::memcpy(mappedData + (dynamicOffsetIndex * atomSize_), data, static_cast<size_t>(dataSize));

    uint32_t const dynamicOffset = static_cast<uint32_t>(dynamicOffsetIndex * atomSize_);
    vkCmdBindDescriptorSets(frameInfo.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, kMaterialSetIndex, 1, &descriptorSets_[frameInfo.frameIndex], 1, &dynamicOffset);

    dynamicOffsetIndex++;
  }

  void MaterialRenderBindings::bindMaterial(FrameInfo& frameInfo, VkPipelineLayout pipelineLayout, const PBRMaterial* material, float isSelected)
  {
    MaterialUniformData const matData = buildMaterialUniformData(material, isSelected);
    writeAndBind(frameInfo, pipelineLayout, &matData, sizeof(MaterialUniformData));
  }

  bool MaterialRenderBindings::needsFullVariant(FrameInfo const& frameInfo, const PBRMaterial* mat)
  {
    if (frameInfo.debugMode != 0)
    {
      return true;
    }
    if (mat == nullptr)
    {
      return false;
    }
    if (mat->useSpecularGlossinessWorkflow)
    {
      return true;
    }
    if (mat->iridescence > kFeatureEpsCpu)
    {
      return true;
    }
    if (mat->transmission > kFeatureEpsCpu || mat->hasTransmissionMap())
    {
      return true;
    }
    if (mat->clearcoat > kFeatureEpsCpu || mat->hasClearcoatMap() || mat->hasClearcoatRoughnessMap() || mat->hasClearcoatNormalMap())
    {
      return true;
    }
    if (mat->anisotropic > kFeatureEpsCpu || mat->anisotropic < -kFeatureEpsCpu)
    {
      return true;
    }
    return false;
  }

} // namespace engine
