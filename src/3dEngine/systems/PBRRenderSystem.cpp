#include "3dEngine/systems/PBRRenderSystem.hpp"

#include "3dEngine/Exceptions.hpp"
#include "3dEngine/MorphTargetManager.hpp"
#include "3dEngine/PBRMaterial.hpp"
#include "3dEngine/Texture.hpp"

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
    createDefaultTextures();
    createMaterialDescriptorSetLayout();
    createMaterialDescriptorPool();
    createPipelineLayout(globalSetLayout);
    createPipeline(renderPass);
  }

  void PBRRenderSystem::createDefaultTextures()
  {
    defaultWhiteTexture  = Texture::createWhiteTexture(device);
    defaultNormalTexture = Texture::createNormalTexture(device);
  }

  void PBRRenderSystem::createMaterialDescriptorSetLayout()
  {
    materialSetLayout = DescriptorSetLayout::Builder(device)
                                .addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT) // albedo
                                .addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT) // normal
                                .addBinding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT) // metallic
                                .addBinding(3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT) // roughness
                                .addBinding(4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT) // ao
                                .build();
  }

  void PBRRenderSystem::createMaterialDescriptorPool()
  {
    materialDescriptorPool = DescriptorPool::Builder(device)
                                     .setMaxSets(1000) // Support many materials
                                     .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 5000)
                                     .build();
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

    // Set 0: global (camera, lights), Set 1: material (textures)
    std::vector<VkDescriptorSetLayout> descriptorSetLayouts{globalSetLayout, materialSetLayout->getDescriptorSetLayout()};

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

  PBRRenderSystem::~PBRRenderSystem()
  {
    vkDestroyPipelineLayout(device.device(), pipelineLayout, nullptr);
  }

  VkDescriptorSet PBRRenderSystem::getMaterialDescriptorSet(const PBRMaterial& material)
  {
    // Create a hash from material pointer (simple cache key)
    size_t materialHash = reinterpret_cast<size_t>(&material);

    // Check cache
    auto it = materialDescriptorCache.find(materialHash);
    if (it != materialDescriptorCache.end())
    {
      return it->second;
    }

    // Create new descriptor set
    VkDescriptorSet descriptorSet;
    if (!materialDescriptorPool->allocateDescriptor(materialSetLayout->getDescriptorSetLayout(), descriptorSet))
    {
      throw std::runtime_error("Failed to allocate material descriptor set!");
    }

    // Write descriptor set with textures or default fallbacks
    DescriptorWriter writer(*materialSetLayout, *materialDescriptorPool);

    // Albedo map (binding 0)
    VkDescriptorImageInfo albedoInfo = material.albedoMap ? material.albedoMap->getDescriptorInfo() : defaultWhiteTexture->getDescriptorInfo();
    writer.writeImage(0, &albedoInfo);

    // Normal map (binding 1)
    VkDescriptorImageInfo normalInfo = material.normalMap ? material.normalMap->getDescriptorInfo() : defaultNormalTexture->getDescriptorInfo();
    writer.writeImage(1, &normalInfo);

    // Metallic map (binding 2)
    VkDescriptorImageInfo metallicInfo = material.metallicMap ? material.metallicMap->getDescriptorInfo() : defaultWhiteTexture->getDescriptorInfo();
    writer.writeImage(2, &metallicInfo);

    // Roughness map (binding 3)
    VkDescriptorImageInfo roughnessInfo = material.roughnessMap ? material.roughnessMap->getDescriptorInfo() : defaultWhiteTexture->getDescriptorInfo();
    writer.writeImage(3, &roughnessInfo);

    // AO map (binding 4)
    VkDescriptorImageInfo aoInfo = material.aoMap ? material.aoMap->getDescriptorInfo() : defaultWhiteTexture->getDescriptorInfo();
    writer.writeImage(4, &aoInfo);

    writer.overwrite(descriptorSet);

    // Cache the descriptor set
    materialDescriptorCache[materialHash] = descriptorSet;

    return descriptorSet;
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

    // Track last bound descriptor to avoid redundant bindings
    VkDescriptorSet lastBoundMaterial = VK_NULL_HANDLE;

    // Build list of visible objects with frustum culling
    struct RenderObject
    {
      GameObject::id_t   id;
      const GameObject*  obj;
      const PBRMaterial* material; // For single-material objects
      VkDescriptorSet    materialDescSet;
      float              distanceToCamera; // For sorting
    };

    std::vector<RenderObject> visibleObjects;
    visibleObjects.reserve(frameInfo.gameObjects.size());

    for (const auto& [id, gameObject] : frameInfo.gameObjects)
    {
      // Skip objects without models
      if (!gameObject.model)
      {
        continue;
      }

      // Check if this is a multi-material model or a PBR object
      bool isMultiMaterial = gameObject.model->hasMultipleMaterials();
      bool hasPBRMaterial  = gameObject.pbrMaterial != nullptr;

      // Skip if neither multi-material nor has PBR material
      if (!isMultiMaterial && !hasPBRMaterial)
      {
        continue;
      }

      // Frustum culling: check if object is visible
      glm::vec3 center;
      float     radius;
      gameObject.getBoundingSphere(center, radius);
      if (!frameInfo.camera.isInFrustum(center, radius))
      {
        continue; // Skip objects outside view frustum
      }

      // Calculate distance to camera for sorting
      float distanceToCamera = glm::length(frameInfo.camera.getPosition() - center);

      // Add to visible list
      if (!isMultiMaterial && hasPBRMaterial)
      {
        // Single-material objects can be sorted
        visibleObjects.push_back({id, &gameObject, gameObject.pbrMaterial.get(), VK_NULL_HANDLE, distanceToCamera});
      }
      else
      {
        // Multi-material objects (just add, they handle their own materials)
        visibleObjects.push_back({id, &gameObject, nullptr, VK_NULL_HANDLE, distanceToCamera});
      }
    }

    // Sort by material to minimize state changes (single-material objects only)
    // Multi-material objects will be rendered in original order
    std::stable_sort(visibleObjects.begin(), visibleObjects.end(), [](const RenderObject& a, const RenderObject& b) {
      // Sort single-material objects by material pointer, multi-material objects at end
      if (a.material && b.material)
      {
        return a.material < b.material;
      }
      return a.material > b.material; // Put multi-material objects at end
    });

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
          VkDescriptorSet materialDescSet = getMaterialDescriptorSet(material);

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
        VkDescriptorSet materialDescSet = getMaterialDescriptorSet(material);

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
