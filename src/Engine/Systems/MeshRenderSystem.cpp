#include "Engine/Systems/MeshRenderSystem.hpp"

#include "Engine/Core/Exceptions.hpp"
#include "Engine/Graphics/SwapChain.hpp"
#include "Engine/Resources/PBRMaterial.hpp"
#include "Engine/Resources/Texture.hpp"
#include "Engine/Scene/components/ModelComponent.hpp"
#include "Engine/Scene/components/TransformComponent.hpp"
#include "Engine/Systems/IBLSystem.hpp"
#include "Engine/Systems/ShadowSystem.hpp"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <algorithm>
#include <array>
#include <cstring> // for memcpy
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
namespace engine {

  struct MeshPushConstantData
  {
    glm::mat4 modelMatrix{1.0f};
    glm::mat4 normalMatrix{1.0f};
    uint32_t  meshId{0};

    uint64_t  meshletBufferAddress;
    uint64_t  meshletVerticesAddress;
    uint64_t  meshletTrianglesAddress;
    uint64_t  vertexBufferAddress;
    uint32_t  meshletOffset;
    uint32_t  meshletCount;
    glm::vec2 screenSize;
    uint32_t  cullingFlags; // Bit 0: Double Sided
  };

  MeshRenderSystem::MeshRenderSystem(Device& device, VkRenderPass renderPass, VkDescriptorSetLayout globalSetLayout, VkDescriptorSetLayout bindlessSetLayout)
      : device(device)
  {
    createShadowDescriptorResources();
    createIBLDescriptorResources();
    createMaterialDescriptorResources();
    createPipelineLayout(globalSetLayout, bindlessSetLayout);
    createPipeline(renderPass);
  }

  MeshRenderSystem::~MeshRenderSystem()
  {
    vkDestroyPipelineLayout(device.device(), pipelineLayout, nullptr);
    if (shadowDescriptorPool_ != VK_NULL_HANDLE)
    {
      vkDestroyDescriptorPool(device.device(), shadowDescriptorPool_, nullptr);
    }
    if (shadowDescriptorSetLayout_ != VK_NULL_HANDLE)
    {
      vkDestroyDescriptorSetLayout(device.device(), shadowDescriptorSetLayout_, nullptr);
    }
    if (iblDescriptorPool_ != VK_NULL_HANDLE)
    {
      vkDestroyDescriptorPool(device.device(), iblDescriptorPool_, nullptr);
    }
    if (iblDescriptorSetLayout_ != VK_NULL_HANDLE)
    {
      vkDestroyDescriptorSetLayout(device.device(), iblDescriptorSetLayout_, nullptr);
    }
    if (materialDescriptorPool_ != VK_NULL_HANDLE)
    {
      vkDestroyDescriptorPool(device.device(), materialDescriptorPool_, nullptr);
    }
    if (materialDescriptorSetLayout_ != VK_NULL_HANDLE)
    {
      vkDestroyDescriptorSetLayout(device.device(), materialDescriptorSetLayout_, nullptr);
    }
  }

  void MeshRenderSystem::createShadowDescriptorResources()
  {
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

    if (vkCreateDescriptorSetLayout(device.device(), &layoutInfo, nullptr, &shadowDescriptorSetLayout_) != VK_SUCCESS)
    {
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

    if (vkCreateDescriptorPool(device.device(), &poolInfo, nullptr, &shadowDescriptorPool_) != VK_SUCCESS)
    {
      throw std::runtime_error("Failed to create shadow descriptor pool");
    }

    std::vector<VkDescriptorSetLayout> layouts(SwapChain::maxFramesInFlight(), shadowDescriptorSetLayout_);

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool     = shadowDescriptorPool_;
    allocInfo.descriptorSetCount = static_cast<uint32_t>(SwapChain::maxFramesInFlight());
    allocInfo.pSetLayouts        = layouts.data();

    shadowDescriptorSets_.resize(SwapChain::maxFramesInFlight());
    if (vkAllocateDescriptorSets(device.device(), &allocInfo, shadowDescriptorSets_.data()) != VK_SUCCESS)
    {
      throw std::runtime_error("Failed to allocate shadow descriptor sets");
    }
  }

  void MeshRenderSystem::createIBLDescriptorResources()
  {
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

    if (vkCreateDescriptorSetLayout(device.device(), &layoutInfo, nullptr, &iblDescriptorSetLayout_) != VK_SUCCESS)
    {
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

    if (vkCreateDescriptorPool(device.device(), &poolInfo, nullptr, &iblDescriptorPool_) != VK_SUCCESS)
    {
      throw std::runtime_error("Failed to create IBL descriptor pool");
    }

    std::vector<VkDescriptorSetLayout> layouts(SwapChain::maxFramesInFlight(), iblDescriptorSetLayout_);

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool     = iblDescriptorPool_;
    allocInfo.descriptorSetCount = static_cast<uint32_t>(SwapChain::maxFramesInFlight());
    allocInfo.pSetLayouts        = layouts.data();

    iblDescriptorSets_.resize(SwapChain::maxFramesInFlight());
    if (vkAllocateDescriptorSets(device.device(), &allocInfo, iblDescriptorSets_.data()) != VK_SUCCESS)
    {
      throw std::runtime_error("Failed to allocate IBL descriptor sets");
    }
  }

  void MeshRenderSystem::createMaterialDescriptorResources()
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

    if (vkCreateDescriptorSetLayout(device.device(), &layoutInfo, nullptr, &materialDescriptorSetLayout_) != VK_SUCCESS)
    {
      throw std::runtime_error("Failed to create material descriptor set layout");
    }

    VkDeviceSize minAlignment = device.getProperties().limits.minUniformBufferOffsetAlignment;
    VkDeviceSize atomSize     = sizeof(MaterialUniformData);
    if (minAlignment > 0)
    {
      atomSize = (atomSize + minAlignment - 1) & ~(minAlignment - 1);
    }

    materialBuffers_.resize(SwapChain::maxFramesInFlight());
    for (size_t i = 0; i < SwapChain::maxFramesInFlight(); i++)
    {
      materialBuffers_[i] = std::make_unique<Buffer>(device,
                                                     atomSize,
                                                     10000, // Max objects assumption
                                                     VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
      materialBuffers_[i]->map();
    }

    VkDescriptorPoolSize poolSize{};
    poolSize.type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    poolSize.descriptorCount = static_cast<uint32_t>(SwapChain::maxFramesInFlight());

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes    = &poolSize;
    poolInfo.maxSets       = static_cast<uint32_t>(SwapChain::maxFramesInFlight());

    if (vkCreateDescriptorPool(device.device(), &poolInfo, nullptr, &materialDescriptorPool_) != VK_SUCCESS)
    {
      throw std::runtime_error("Failed to create material descriptor pool");
    }

    std::vector<VkDescriptorSetLayout> layouts(SwapChain::maxFramesInFlight(), materialDescriptorSetLayout_);

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool     = materialDescriptorPool_;
    allocInfo.descriptorSetCount = static_cast<uint32_t>(SwapChain::maxFramesInFlight());
    allocInfo.pSetLayouts        = layouts.data();

    materialDescriptorSets_.resize(SwapChain::maxFramesInFlight());
    if (vkAllocateDescriptorSets(device.device(), &allocInfo, materialDescriptorSets_.data()) != VK_SUCCESS)
    {
      throw std::runtime_error("Failed to allocate material descriptor sets");
    }

    for (size_t i = 0; i < SwapChain::maxFramesInFlight(); i++)
    {
      VkDescriptorBufferInfo bufferInfo = materialBuffers_[i]->descriptorInfoForIndex(0);

      VkWriteDescriptorSet descriptorWrite{};
      descriptorWrite.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      descriptorWrite.dstSet          = materialDescriptorSets_[i];
      descriptorWrite.dstBinding      = 0;
      descriptorWrite.dstArrayElement = 0;
      descriptorWrite.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
      descriptorWrite.descriptorCount = 1;
      descriptorWrite.pBufferInfo     = &bufferInfo;

      vkUpdateDescriptorSets(device.device(), 1, &descriptorWrite, 0, nullptr);
    }
  }

  void MeshRenderSystem::createPipelineLayout(VkDescriptorSetLayout globalSetLayout, VkDescriptorSetLayout bindlessSetLayout)
  {
    VkPushConstantRange pushConstantRange{
            .stageFlags = VK_SHADER_STAGE_TASK_BIT_EXT | VK_SHADER_STAGE_MESH_BIT_EXT | VK_SHADER_STAGE_FRAGMENT_BIT,
            .offset     = 0,
            .size       = sizeof(MeshPushConstantData),
    };

    std::vector<VkDescriptorSetLayout> descriptorSetLayouts{globalSetLayout,
                                                            bindlessSetLayout,
                                                            shadowDescriptorSetLayout_,
                                                            iblDescriptorSetLayout_,
                                                            materialDescriptorSetLayout_};

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{
            .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount         = static_cast<uint32_t>(descriptorSetLayouts.size()),
            .pSetLayouts            = descriptorSetLayouts.data(),
            .pushConstantRangeCount = 1,
            .pPushConstantRanges    = &pushConstantRange,
    };
    if (vkCreatePipelineLayout(device.device(), &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS)
    {
      throw engine::RuntimeException("failed to create pipeline layout!");
    }
  }

  void MeshRenderSystem::createPipeline(VkRenderPass renderPass)
  {
    assert(pipelineLayout != VK_NULL_HANDLE && "Pipeline layout must be created before pipeline.");

    PipelineConfigInfo pipelineConfig{};
    Pipeline::defaultMeshPipelineConfigInfo(pipelineConfig);

    pipelineConfig.renderPass     = renderPass;
    pipelineConfig.pipelineLayout = pipelineLayout;

    pipeline = std::make_unique<Pipeline>(device,
                                          SHADER_PATH "/simple_mesh.task.spv",
                                          SHADER_PATH "/simple_mesh.mesh.spv",
                                          SHADER_PATH "/pbr_shader.frag.spv",
                                          pipelineConfig);

    // Create Transparent Pipeline
    PipelineConfigInfo transparentConfig                       = pipelineConfig;
    transparentConfig.colorBlendAttachment.blendEnable         = VK_TRUE;
    transparentConfig.colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    transparentConfig.colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    transparentConfig.colorBlendAttachment.colorBlendOp        = VK_BLEND_OP_ADD;
    transparentConfig.colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    transparentConfig.colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    transparentConfig.colorBlendAttachment.alphaBlendOp        = VK_BLEND_OP_ADD;

    // Fix pointer to attachment
    transparentConfig.colorBlendInfo.pAttachments = &transparentConfig.colorBlendAttachment;

    // Disable depth write for transparent objects
    transparentConfig.depthStencilInfo.depthWriteEnable = VK_FALSE;

    transparentPipeline = std::make_unique<Pipeline>(device,
                                                     SHADER_PATH "/simple_mesh.task.spv",
                                                     SHADER_PATH "/simple_mesh.mesh.spv",
                                                     SHADER_PATH "/pbr_shader.frag.spv",
                                                     transparentConfig);
  }

  void MeshRenderSystem::setShadowSystem(ShadowSystem* shadowSystem)
  {
    currentShadowSystem_ = shadowSystem;
  }

  void MeshRenderSystem::setIBLSystem(IBLSystem* iblSystem)
  {
    currentIBLSystem_ = iblSystem;
  }

  void MeshRenderSystem::render(FrameInfo& frameInfo)
  {
    pipeline->bind(frameInfo.commandBuffer);

    vkCmdBindDescriptorSets(frameInfo.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &frameInfo.globalDescriptorSet, 0, nullptr);
    vkCmdBindDescriptorSets(frameInfo.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 1, 1, &frameInfo.globalTextureSet, 0, nullptr);

    // Bind Shadow Maps
    if (currentShadowSystem_)
    {
      int shadowCount     = currentShadowSystem_->getShadowLightCount();
      int cubeShadowCount = currentShadowSystem_->getCubeShadowLightCount();

      std::array<VkDescriptorImageInfo, ShadowSystem::MAX_SHADOW_MAPS> shadowInfos{};
      for (int i = 0; i < shadowCount && i < ShadowSystem::MAX_SHADOW_MAPS; i++)
      {
        shadowInfos[i] = currentShadowSystem_->getShadowMapDescriptorInfo(i);
      }
      for (int i = shadowCount; i < ShadowSystem::MAX_SHADOW_MAPS; i++)
      {
        shadowInfos[i] = shadowCount > 0 ? currentShadowSystem_->getShadowMapDescriptorInfo(0) : currentShadowSystem_->getShadowMapDescriptorInfo(0);
      }

      std::array<VkDescriptorImageInfo, ShadowSystem::MAX_CUBE_SHADOW_MAPS> cubeShadowInfos{};
      for (int i = 0; i < cubeShadowCount && i < ShadowSystem::MAX_CUBE_SHADOW_MAPS; i++)
      {
        cubeShadowInfos[i] = currentShadowSystem_->getCubeShadowMapDescriptorInfo(i);
      }
      for (int i = cubeShadowCount; i < ShadowSystem::MAX_CUBE_SHADOW_MAPS; i++)
      {
        cubeShadowInfos[i] = cubeShadowCount > 0 ? currentShadowSystem_->getCubeShadowMapDescriptorInfo(0)
                                                 : currentShadowSystem_->getCubeShadowMapDescriptorInfo(0);
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

      vkUpdateDescriptorSets(device.device(), static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
      vkCmdBindDescriptorSets(frameInfo.commandBuffer,
                              VK_PIPELINE_BIND_POINT_GRAPHICS,
                              pipelineLayout,
                              2,
                              1,
                              &shadowDescriptorSets_[frameInfo.frameIndex],
                              0,
                              nullptr);
    }

    // Bind IBL
    if (currentIBLSystem_ && currentIBLSystem_->isGenerated())
    {
      VkDescriptorImageInfo irradianceInfo = currentIBLSystem_->getIrradianceDescriptorInfo();
      VkDescriptorImageInfo prefilterInfo  = currentIBLSystem_->getPrefilteredDescriptorInfo();
      VkDescriptorImageInfo brdfInfo       = currentIBLSystem_->getBRDFLUTDescriptorInfo();

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

      vkUpdateDescriptorSets(device.device(), static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
      vkCmdBindDescriptorSets(frameInfo.commandBuffer,
                              VK_PIPELINE_BIND_POINT_GRAPHICS,
                              pipelineLayout,
                              3,
                              1,
                              &iblDescriptorSets_[frameInfo.frameIndex],
                              0,
                              nullptr);
    }

    // Material Buffer Preparation
    VkDeviceSize minAlignment = device.getProperties().limits.minUniformBufferOffsetAlignment;
    VkDeviceSize atomSize     = sizeof(MaterialUniformData);
    if (minAlignment > 0)
    {
      atomSize = (atomSize + minAlignment - 1) & ~(minAlignment - 1);
    }

    char*    mappedData         = (char*)materialBuffers_[frameInfo.frameIndex]->getMappedMemory();
    uint32_t dynamicOffsetIndex = 0;

    auto view = frameInfo.scene->getRegistry().view<ModelComponent, TransformComponent>();

    struct TransparentRenderItem
    {
      entt::entity          entity;
      const Model::SubMesh* subMesh;
      const PBRMaterial*    material;
      glm::mat4             modelMatrix;
      float                 distance;
    };

    std::vector<TransparentRenderItem> transparentItems;

    // Helper to render a single item
    auto renderItem = [&](entt::entity entity, const Model::SubMesh& subMesh, const PBRMaterial* pMaterial, const glm::mat4& modelMatrix) {
      if (dynamicOffsetIndex >= 10000) return;

      auto& modelComp = view.get<ModelComponent>(entity);

      MeshPushConstantData push{};
      push.modelMatrix             = modelMatrix;
      push.normalMatrix            = glm::transpose(glm::inverse(push.modelMatrix));
      push.meshId                  = modelComp.model->getMeshId();
      push.meshletBufferAddress    = modelComp.model->getMeshletBufferAddress();
      push.meshletVerticesAddress  = modelComp.model->getMeshletVerticesAddress();
      push.meshletTrianglesAddress = modelComp.model->getMeshletTrianglesAddress();
      push.vertexBufferAddress     = modelComp.model->getVertexBufferAddress();
      push.meshletOffset           = subMesh.meshletOffset;
      push.meshletCount            = subMesh.meshletCount;
      push.screenSize              = glm::vec2(frameInfo.extent.width, frameInfo.extent.height);

      if (pMaterial && pMaterial->doubleSided)
      {
        push.cullingFlags = 1;
      }
      else
      {
        push.cullingFlags = 0;
      }

      MaterialUniformData matData{};
      float               isSelected = ((uint32_t)entity == frameInfo.selectedObjectId) ? 1.0f : 0.0f;

      if (pMaterial)
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

        // Pack floats into mat4
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

        // Pack uints
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

        // Defaults
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

      memcpy(mappedData + (dynamicOffsetIndex * atomSize), &matData, sizeof(MaterialUniformData));

      uint32_t dynamicOffset = static_cast<uint32_t>(dynamicOffsetIndex * atomSize);
      vkCmdBindDescriptorSets(frameInfo.commandBuffer,
                              VK_PIPELINE_BIND_POINT_GRAPHICS,
                              pipelineLayout,
                              4,
                              1,
                              &materialDescriptorSets_[frameInfo.frameIndex],
                              1,
                              &dynamicOffset);

      dynamicOffsetIndex++;

      vkCmdPushConstants(frameInfo.commandBuffer,
                         pipelineLayout,
                         VK_SHADER_STAGE_TASK_BIT_EXT | VK_SHADER_STAGE_MESH_BIT_EXT | VK_SHADER_STAGE_FRAGMENT_BIT,
                         0,
                         sizeof(MeshPushConstantData),
                         &push);

      if (device.vkCmdDrawMeshTasksEXT)
      {
        uint32_t groupCount = (subMesh.meshletCount + 31) / 32;
        device.vkCmdDrawMeshTasksEXT(frameInfo.commandBuffer, groupCount, 1, 1);
      }
    };

    // 1. Render Opaque Objects
    for (auto entity : view)
    {
      auto [modelComp, transform] = view.get<ModelComponent, TransformComponent>(entity);
      if (!modelComp.model) continue;

      const auto& subMeshes = modelComp.model->getSubMeshes();
      const auto& materials = modelComp.model->getMaterials();

      for (const auto& subMesh : subMeshes)
      {
        if (subMesh.meshletCount == 0) continue;

        const PBRMaterial* pMaterial = nullptr;
        if (auto* mat = frameInfo.scene->getRegistry().try_get<PBRMaterial>(entity))
        {
          pMaterial = mat;
        }
        else if (subMesh.materialId >= 0 && subMesh.materialId < materials.size())
        {
          pMaterial = &materials[subMesh.materialId].pbrMaterial;
        }

        bool isTransparent = false;
        if (pMaterial)
        {
          if (pMaterial->alphaMode == AlphaMode::Blend || pMaterial->transmission > 0.0f)
          {
            isTransparent = true;
          }
        }

        if (!isTransparent)
        {
          renderItem(entity, subMesh, pMaterial, transform.modelTransform());
        }
        else
        {
          // Collect transparent item
          glm::vec3 worldPos = glm::vec3(transform.modelTransform()[3]);
          float     dist     = glm::distance(worldPos, frameInfo.camera.getPosition());
          transparentItems.push_back({entity, &subMesh, pMaterial, transform.modelTransform(), dist});
        }
      }
    }

    // 2. Sort Transparent Objects (Back-to-Front)
    std::sort(transparentItems.begin(), transparentItems.end(), [](const TransparentRenderItem& a, const TransparentRenderItem& b) {
      return a.distance > b.distance;
    });

    // 3. Render Transparent Objects
    transparentPipeline->bind(frameInfo.commandBuffer);
    for (const auto& item : transparentItems)
    {
      renderItem(item.entity, *item.subMesh, item.material, item.modelMatrix);
    }
  }
} // namespace engine
