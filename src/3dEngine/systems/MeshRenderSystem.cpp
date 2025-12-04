#include "3dEngine/systems/MeshRenderSystem.hpp"

#include "3dEngine/Exceptions.hpp"
#include "3dEngine/GameObjectManager.hpp"
#include "3dEngine/IBLSystem.hpp"
#include "3dEngine/SwapChain.hpp"
#include "3dEngine/Texture.hpp"
#include "3dEngine/systems/ShadowSystem.hpp"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <array>
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

namespace engine {

  struct MeshPushConstantData
  {
    glm::mat4 modelMatrix{1.0f};
    glm::mat4 normalMatrix{1.0f};
    glm::vec3 albedo{1.0f, 1.0f, 1.0f};
    float     metallic{0.0f};
    float     roughness{0.5f};
    float     ao{1.0f};
    float     isSelected{0.0f};
    float     clearcoat{0.0f};
    float     clearcoatRoughness{0.03f};
    float     anisotropic{0.0f};
    float     anisotropicRotation{0.0f};
    uint32_t  textureFlags{0};
    float     uvScale{1.0f};
    uint32_t  albedoIndex{0};
    uint32_t  normalIndex{0};
    uint32_t  metallicIndex{0};
    uint32_t  roughnessIndex{0};
    uint32_t  aoIndex{0};
    uint32_t  meshId{0};

    uint64_t  meshletBufferAddress;
    uint64_t  meshletVerticesAddress;
    uint64_t  meshletTrianglesAddress;
    uint64_t  vertexBufferAddress;
    uint32_t  meshletOffset;
    uint32_t  meshletCount;
    glm::vec2 screenSize;
  };

  MeshRenderSystem::MeshRenderSystem(Device& device, VkRenderPass renderPass, VkDescriptorSetLayout globalSetLayout, VkDescriptorSetLayout bindlessSetLayout)
      : device(device)
  {
    createShadowDescriptorResources();
    createIBLDescriptorResources();
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

  void MeshRenderSystem::createPipelineLayout(VkDescriptorSetLayout globalSetLayout, VkDescriptorSetLayout bindlessSetLayout)
  {
    VkPushConstantRange pushConstantRange{
            .stageFlags = VK_SHADER_STAGE_TASK_BIT_EXT | VK_SHADER_STAGE_MESH_BIT_EXT | VK_SHADER_STAGE_FRAGMENT_BIT,
            .offset     = 0,
            .size       = sizeof(MeshPushConstantData),
    };

    std::vector<VkDescriptorSetLayout> descriptorSetLayouts{globalSetLayout, bindlessSetLayout, shadowDescriptorSetLayout_, iblDescriptorSetLayout_};

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

    for (const auto& [id, gameObject] : frameInfo.objectManager->getAllObjects())
    {
      if (!gameObject.model) continue;

      const auto& subMeshes = gameObject.model->getSubMeshes();
      const auto& materials = gameObject.model->getMaterials();

      for (const auto& subMesh : subMeshes)
      {
        if (subMesh.meshletCount == 0) continue;

        MeshPushConstantData push{};
        push.modelMatrix             = gameObject.transform.modelTransform();
        push.normalMatrix            = glm::mat4{gameObject.transform.normalMatrix()};
        push.meshId                  = gameObject.model->getMeshId();
        push.meshletBufferAddress    = gameObject.model->getMeshletBufferAddress();
        push.meshletVerticesAddress  = gameObject.model->getMeshletVerticesAddress();
        push.meshletTrianglesAddress = gameObject.model->getMeshletTrianglesAddress();
        push.vertexBufferAddress     = gameObject.model->getVertexBufferAddress();
        push.meshletOffset           = subMesh.meshletOffset;
        push.meshletCount            = subMesh.meshletCount;
        push.screenSize              = glm::vec2(frameInfo.extent.width, frameInfo.extent.height);
        push.isSelected              = (id == frameInfo.selectedObjectId) ? 1.0f : 0.0f;

        const PBRMaterial* pMaterial = nullptr;
        if (gameObject.pbrMaterial)
        {
          pMaterial = gameObject.pbrMaterial.get();
        }
        else if (subMesh.materialId >= 0 && subMesh.materialId < materials.size())
        {
          pMaterial = &materials[subMesh.materialId].pbrMaterial;
        }

        if (pMaterial)
        {
          const auto& material = *pMaterial;

          uint32_t textureFlags = 0;
          if (material.hasAlbedoMap())
          {
            textureFlags |= (1 << 0);
            push.albedoIndex = material.albedoMap->getGlobalIndex();
          }
          if (material.hasNormalMap())
          {
            textureFlags |= (1 << 1);
            push.normalIndex = material.normalMap->getGlobalIndex();
          }
          if (material.hasMetallicMap())
          {
            textureFlags |= (1 << 2);
            push.metallicIndex = material.metallicMap->getGlobalIndex();
          }
          if (material.hasRoughnessMap())
          {
            textureFlags |= (1 << 3);
            push.roughnessIndex = material.roughnessMap->getGlobalIndex();
          }
          if (material.hasAOMap())
          {
            textureFlags |= (1 << 4);
            push.aoIndex = material.aoMap->getGlobalIndex();
          }

          push.albedo              = material.albedo;
          push.metallic            = material.metallic;
          push.roughness           = material.roughness;
          push.ao                  = material.ao;
          push.clearcoat           = material.clearcoat;
          push.clearcoatRoughness  = material.clearcoatRoughness;
          push.anisotropic         = material.anisotropic;
          push.anisotropicRotation = material.anisotropicRotation;
          push.textureFlags        = textureFlags;
          push.uvScale             = material.uvScale;
        }
        else
        {
          // Default material
          push.albedo    = glm::vec3(1.0f);
          push.metallic  = 0.0f;
          push.roughness = 0.5f;
          push.ao        = 1.0f;
        }

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
      }
    }
  }
} // namespace engine
