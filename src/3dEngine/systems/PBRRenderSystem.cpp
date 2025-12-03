#include "3dEngine/systems/PBRRenderSystem.hpp"

#include "3dEngine/CubeShadowMap.hpp"
#include "3dEngine/Exceptions.hpp"
#include "3dEngine/GameObjectManager.hpp"
#include "3dEngine/MorphTargetManager.hpp"
#include "3dEngine/PBRMaterial.hpp"
#include "3dEngine/ShadowMap.hpp"
#include "3dEngine/Texture.hpp"
#include "3dEngine/systems/MaterialSystem.hpp"
#include "3dEngine/systems/RenderCullingSystem.hpp"
#include "3dEngine/systems/ShadowSystem.hpp"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <GLFW/glfw3.h>

#include <array>
#include <cmath>
#include <glm/common.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <stdexcept>

namespace engine {

  struct PBRPushConstantData
  {
    glm::mat4 modelMatrix{1.0f};
    glm::mat4 normalMatrix{1.0f};
    glm::vec3 albedo{1.0f, 1.0f, 1.0f};
    float     metallic{0.0f};
    float     roughness{0.5f};
    float     ao{1.0f};
    float     isSelected{0.0f};          // 1.0 if selected, 0.0 otherwise
    float     clearcoat{0.0f};           // Clearcoat layer strength
    float     clearcoatRoughness{0.03f}; // Clearcoat roughness
    float     anisotropic{0.0f};         // Anisotropy strength
    float     anisotropicRotation{0.0f}; // Anisotropic direction rotation
    uint32_t  textureFlags{0};           // Bit flags for which textures are present
    float     uvScale{1.0f};             // UV tiling scale
    float     padding[2];                // Align to 16 bytes
  };

  PBRRenderSystem::PBRRenderSystem(Device& device, VkRenderPass renderPass, VkDescriptorSetLayout globalSetLayout) : device(device)
  {
    materialSystem = std::make_unique<MaterialSystem>(device);
    createShadowDescriptorResources();
    createPipelineLayout(globalSetLayout);
    createPipeline(renderPass);
  }

  PBRRenderSystem::~PBRRenderSystem()
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
  }

  void PBRRenderSystem::clearDescriptorCache()
  {
    materialSystem->clearDescriptorCache();
  }

  void PBRRenderSystem::createPipelineLayout(VkDescriptorSetLayout globalSetLayout)
  {
    // Single push constant range covering both transform and material data
    // Both vertex and fragment shaders can access it
    VkPushConstantRange pushConstantRange{
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            .offset     = 0,
            .size       = sizeof(PBRPushConstantData),
    };

    // Set 0: global (camera, lights), Set 1: material (textures), Set 2: shadow map
    std::vector<VkDescriptorSetLayout> descriptorSetLayouts{globalSetLayout, materialSystem->getDescriptorSetLayout(), shadowDescriptorSetLayout_};

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

  void PBRRenderSystem::createShadowDescriptorResources()
  {
    // Create shadow descriptor set layout with array of shadow maps and cube shadow maps
    std::array<VkDescriptorSetLayoutBinding, 2> bindings{};

    // Binding 0: 2D shadow maps for directional and spot lights
    bindings[0].binding            = 0;
    bindings[0].descriptorType     = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[0].descriptorCount    = MAX_SHADOW_MAPS;
    bindings[0].stageFlags         = VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings[0].pImmutableSamplers = nullptr;

    // Binding 1: Cube shadow maps for point lights
    bindings[1].binding            = 1;
    bindings[1].descriptorType     = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[1].descriptorCount    = MAX_CUBE_SHADOW_MAPS;
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

    // Create descriptor pool with enough descriptors for all shadow maps
    std::array<VkDescriptorPoolSize, 2> poolSizes{};
    poolSizes[0].type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[0].descriptorCount = static_cast<uint32_t>(SwapChain::maxFramesInFlight() * MAX_SHADOW_MAPS);
    poolSizes[1].type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[1].descriptorCount = static_cast<uint32_t>(SwapChain::maxFramesInFlight() * MAX_CUBE_SHADOW_MAPS);

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes    = poolSizes.data();
    poolInfo.maxSets       = static_cast<uint32_t>(SwapChain::maxFramesInFlight());

    if (vkCreateDescriptorPool(device.device(), &poolInfo, nullptr, &shadowDescriptorPool_) != VK_SUCCESS)
    {
      throw std::runtime_error("Failed to create shadow descriptor pool");
    }

    // Allocate descriptor sets
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

  void PBRRenderSystem::setShadowMap(ShadowMap* shadowMap)
  {
    currentShadowMap_ = shadowMap;
  }

  void PBRRenderSystem::setShadowSystem(ShadowSystem* shadowSystem)
  {
    currentShadowSystem_ = shadowSystem;
  }

  void PBRRenderSystem::createPipeline(VkRenderPass renderPass)
  {
    assert(pipelineLayout != VK_NULL_HANDLE && "Pipeline layout must be created before pipeline.");

    PipelineConfigInfo pipelineConfig{};
    Pipeline::defaultPipelineConfigInfo(pipelineConfig);

    pipelineConfig.renderPass     = renderPass;
    pipelineConfig.pipelineLayout = pipelineLayout;
    pipeline                      = std::make_unique<Pipeline>(device, SHADER_PATH "/pbr_shader.vert.spv", SHADER_PATH "/pbr_shader.frag.spv", pipelineConfig);
  }

  void PBRRenderSystem::render(FrameInfo& frameInfo)
  {
    pipeline->bind(frameInfo.commandBuffer);

    // Bind set 0: global (camera, lights)
    vkCmdBindDescriptorSets(frameInfo.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &frameInfo.globalDescriptorSet, 0, nullptr);

    // Bind set 2: shadow maps (if available)
    if (currentShadowSystem_)
    {
      int shadowCount     = currentShadowSystem_->getShadowLightCount();
      int cubeShadowCount = currentShadowSystem_->getCubeShadowLightCount();

      // Fill in 2D shadow maps
      std::array<VkDescriptorImageInfo, MAX_SHADOW_MAPS> shadowInfos{};
      for (int i = 0; i < shadowCount && i < MAX_SHADOW_MAPS; i++)
      {
        shadowInfos[i] = currentShadowSystem_->getShadowMapDescriptorInfo(i);
      }
      // Fill remaining slots with first shadow map
      for (int i = shadowCount; i < MAX_SHADOW_MAPS; i++)
      {
        shadowInfos[i] = shadowCount > 0 ? currentShadowSystem_->getShadowMapDescriptorInfo(0) : currentShadowSystem_->getShadowMapDescriptorInfo(0);
      }

      // Fill in cube shadow maps
      std::array<VkDescriptorImageInfo, MAX_CUBE_SHADOW_MAPS> cubeShadowInfos{};
      for (int i = 0; i < cubeShadowCount && i < MAX_CUBE_SHADOW_MAPS; i++)
      {
        cubeShadowInfos[i] = currentShadowSystem_->getCubeShadowMapDescriptorInfo(i);
      }
      // Fill remaining slots with first cube shadow map
      for (int i = cubeShadowCount; i < MAX_CUBE_SHADOW_MAPS; i++)
      {
        cubeShadowInfos[i] = cubeShadowCount > 0 ? currentShadowSystem_->getCubeShadowMapDescriptorInfo(0)
                                                 : currentShadowSystem_->getCubeShadowMapDescriptorInfo(0);
      }

      std::array<VkWriteDescriptorSet, 2> descriptorWrites{};

      // Write 2D shadow maps
      descriptorWrites[0].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      descriptorWrites[0].dstSet          = shadowDescriptorSets_[frameInfo.frameIndex];
      descriptorWrites[0].dstBinding      = 0;
      descriptorWrites[0].dstArrayElement = 0;
      descriptorWrites[0].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
      descriptorWrites[0].descriptorCount = MAX_SHADOW_MAPS;
      descriptorWrites[0].pImageInfo      = shadowInfos.data();

      // Write cube shadow maps
      descriptorWrites[1].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      descriptorWrites[1].dstSet          = shadowDescriptorSets_[frameInfo.frameIndex];
      descriptorWrites[1].dstBinding      = 1;
      descriptorWrites[1].dstArrayElement = 0;
      descriptorWrites[1].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
      descriptorWrites[1].descriptorCount = MAX_CUBE_SHADOW_MAPS;
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
    else if (currentShadowMap_)
    {
      // Legacy single shadow map support
      std::array<VkDescriptorImageInfo, MAX_SHADOW_MAPS> shadowInfos{};
      shadowInfos[0] = currentShadowMap_->getDescriptorInfo();
      for (int i = 1; i < MAX_SHADOW_MAPS; i++)
      {
        shadowInfos[i] = shadowInfos[0];
      }

      // Create dummy cube shadow map infos (use the 2D shadow map, will be unused)
      std::array<VkDescriptorImageInfo, MAX_CUBE_SHADOW_MAPS> cubeShadowInfos{};
      for (int i = 0; i < MAX_CUBE_SHADOW_MAPS; i++)
      {
        cubeShadowInfos[i] = shadowInfos[0]; // Use 2D shadow map as placeholder
      }

      std::array<VkWriteDescriptorSet, 2> descriptorWrites{};

      descriptorWrites[0].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      descriptorWrites[0].dstSet          = shadowDescriptorSets_[frameInfo.frameIndex];
      descriptorWrites[0].dstBinding      = 0;
      descriptorWrites[0].dstArrayElement = 0;
      descriptorWrites[0].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
      descriptorWrites[0].descriptorCount = MAX_SHADOW_MAPS;
      descriptorWrites[0].pImageInfo      = shadowInfos.data();

      descriptorWrites[1].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      descriptorWrites[1].dstSet          = shadowDescriptorSets_[frameInfo.frameIndex];
      descriptorWrites[1].dstBinding      = 1;
      descriptorWrites[1].dstArrayElement = 0;
      descriptorWrites[1].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
      descriptorWrites[1].descriptorCount = MAX_CUBE_SHADOW_MAPS;
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

    // Get culled and sorted visible objects
    std::vector<RenderCullingSystem::RenderableObject> visibleObjects;
    RenderCullingSystem::cullAndSort(frameInfo, visibleObjects, true);

    // Track last bound descriptor to avoid redundant bindings
    VkDescriptorSet lastBoundMaterial = VK_NULL_HANDLE;

    // Render sorted visible objects
    for (const auto& renderObj : visibleObjects)
    {
      const GameObject& gameObject = *renderObj.obj;
      GameObject::id_t  id         = renderObj.id;

      // Bind vertex buffer (use blended buffer for morph targets)
      if (gameObject.model->hasMorphTargets() && frameInfo.morphManager && frameInfo.morphManager->isModelInitialized(gameObject.model.get()))
      {
        VkBuffer blendedBuffer = frameInfo.morphManager->getBlendedBuffer(gameObject.model.get());
        if (blendedBuffer != VK_NULL_HANDLE)
        {
          gameObject.model->bindAlternateVertexBuffer(frameInfo.commandBuffer, blendedBuffer);
        }
        else
        {
          gameObject.model->bind(frameInfo.commandBuffer);
        }
      }
      else
      {
        gameObject.model->bind(frameInfo.commandBuffer);
      }

      // Check if model has multiple materials (sub-meshes)
      bool isMultiMaterial = gameObject.model->hasMultipleMaterials();
      if (isMultiMaterial)
      {
        // Render each sub-mesh with its own material
        const auto& materials = gameObject.model->getMaterials();
        const auto& subMeshes = gameObject.model->getSubMeshes();

        for (size_t i = 0; i < subMeshes.size(); ++i)
        {
          const auto& subMesh = subMeshes[i];

          const auto& material = materials[subMesh.materialId].pbrMaterial;

          // Get or create material descriptor set
          VkDescriptorSet materialDescSet = materialSystem->getMaterialDescriptorSet(material);

          // Only bind if different from last material (avoid redundant state changes)
          if (materialDescSet != lastBoundMaterial)
          {
            vkCmdBindDescriptorSets(frameInfo.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 1, 1, &materialDescSet, 0, nullptr);
            lastBoundMaterial = materialDescSet;
          }

          // Set texture flags
          uint32_t textureFlags = 0;
          if (material.hasAlbedoMap()) textureFlags |= (1 << 0);
          if (material.hasNormalMap()) textureFlags |= (1 << 1);
          if (material.hasMetallicMap()) textureFlags |= (1 << 2);
          if (material.hasRoughnessMap()) textureFlags |= (1 << 3);
          if (material.hasAOMap()) textureFlags |= (1 << 4);

          PBRPushConstantData push{};
          push.modelMatrix         = gameObject.transform.modelTransform();
          push.normalMatrix        = glm::mat4{gameObject.transform.normalMatrix()};
          push.isSelected          = (id == frameInfo.selectedObjectId) ? 1.0f : 0.0f;
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

          vkCmdPushConstants(frameInfo.commandBuffer,
                             pipelineLayout,
                             VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                             0,
                             sizeof(PBRPushConstantData),
                             &push);

          gameObject.model->drawSubMesh(frameInfo.commandBuffer, i);
        }
      }
      else
      {
        // Single material rendering
        const auto& material = *gameObject.pbrMaterial;

        // Get or create material descriptor set
        VkDescriptorSet materialDescSet = materialSystem->getMaterialDescriptorSet(material);

        // Only bind if different from last material (avoid redundant state changes)
        if (materialDescSet != lastBoundMaterial)
        {
          vkCmdBindDescriptorSets(frameInfo.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 1, 1, &materialDescSet, 0, nullptr);
          lastBoundMaterial = materialDescSet;
        }

        // Set texture flags
        uint32_t textureFlags = 0;
        if (material.hasAlbedoMap()) textureFlags |= (1 << 0);
        if (material.hasNormalMap()) textureFlags |= (1 << 1);
        if (material.hasMetallicMap()) textureFlags |= (1 << 2);
        if (material.hasRoughnessMap()) textureFlags |= (1 << 3);
        if (material.hasAOMap()) textureFlags |= (1 << 4);

        PBRPushConstantData push{};
        push.modelMatrix         = gameObject.transform.modelTransform();
        push.normalMatrix        = glm::mat4{gameObject.transform.normalMatrix()};
        push.albedo              = material.albedo;
        push.metallic            = material.metallic;
        push.roughness           = material.roughness;
        push.ao                  = material.ao;
        push.clearcoat           = material.clearcoat;
        push.clearcoatRoughness  = material.clearcoatRoughness;
        push.anisotropic         = material.anisotropic;
        push.anisotropicRotation = material.anisotropicRotation;
        push.isSelected          = (id == frameInfo.selectedObjectId) ? 1.0f : 0.0f;
        push.textureFlags        = textureFlags;
        push.uvScale             = material.uvScale;

        vkCmdPushConstants(frameInfo.commandBuffer,
                           pipelineLayout,
                           VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                           0,
                           sizeof(PBRPushConstantData),
                           &push);

        gameObject.model->draw(frameInfo.commandBuffer);
      }
    }
  }
} // namespace engine
