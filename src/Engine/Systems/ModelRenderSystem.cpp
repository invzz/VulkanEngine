#include "Engine/Systems/ModelRenderSystem.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "Engine/Core/Exceptions.hpp"
#include "Engine/Graphics/Device.hpp"
#include "Engine/Graphics/FrameInfo.hpp"
#include "Engine/Graphics/Pipeline.hpp"
#include "Engine/Graphics/SwapChain.hpp"
#include "Engine/Scene/components/LightmapComponent.hpp"
#include "Engine/Scene/components/ModelComponent.hpp"
#include "Engine/Scene/components/NameComponent.hpp"
#include "Engine/Scene/components/TransformComponent.hpp"
#include "Engine/Systems/LightingRenderBindings.hpp"
#include "Engine/Systems/MaterialRenderBindings.hpp"
#include "ModelLib/Resources/Model.hpp"
#include "ModelLib/Resources/PBRMaterial.hpp"
#include "ModelLib/Resources/Texture.hpp"
#include "entt/entity/fwd.hpp"
#include "glm/ext/matrix_float4x4.hpp"
#include "glm/ext/vector_float2.hpp"
#include "glm/ext/vector_float3.hpp"
#include "glm/geometric.hpp"
#include "glm/matrix.hpp"
#include "vulkan/vulkan_core.h"

namespace engine {

  // ============================================================================
  // CPU Frustum Culling
  // ============================================================================

  /**
   * @brief Frustum representation using 6 normalized planes
   */
  struct Frustum
  {
    glm::vec4 planes[6]; // Left, Right, Bottom, Top, Near, Far
  };

  /**
   * @brief Extract frustum planes from view-projection matrix (Gribb-Hartmann method)
   */
  inline Frustum extractFrustumFromMatrix(const glm::mat4& vp)
  {
    Frustum   f;
    glm::mat4 vpT  = glm::transpose(vp);
    glm::vec4 row0 = vpT[0];
    glm::vec4 row1 = vpT[1];
    glm::vec4 row2 = vpT[2];
    glm::vec4 row3 = vpT[3];

    f.planes[0] = row3 + row0; // Left
    f.planes[1] = row3 - row0; // Right
    f.planes[2] = row3 + row1; // Bottom
    f.planes[3] = row3 - row1; // Top
    f.planes[4] = row2;        // Near
    f.planes[5] = row3 - row2; // Far

    // Normalize planes
    for (auto& plane : f.planes)
    {
      float len = glm::length(glm::vec3(plane));
      plane /= len;
    }

    return f;
  }

  /**
   * @brief Test if AABB is inside or intersecting frustum
   * @return true if visible (should be rendered), false if completely outside
   */
  inline bool aabbInFrustum(const AABB& box, const Frustum& f)
  {
    for (int i = 0; i < 6; ++i)
    {
      glm::vec3 normal(f.planes[i]);
      float     d = f.planes[i].w;

      // Find the positive vertex (farthest in plane normal direction)
      glm::vec3 pVertex;
      pVertex.x = (normal.x >= 0.0f) ? box.max.x : box.min.x;
      pVertex.y = (normal.y >= 0.0f) ? box.max.y : box.min.y;
      pVertex.z = (normal.z >= 0.0f) ? box.max.z : box.min.z;

      // If positive vertex is outside this plane, AABB is completely outside frustum
      if (glm::dot(normal, pVertex) + d < 0.0f)
      {
        return false;
      }
    }
    return true;
  }

  /**
   * @brief Check if an entity with model and transform is visible in the camera frustum
   */
  inline bool isEntityVisible(const Model* model, const glm::mat4& modelMatrix, const Frustum& frustum)
  {
    if (model == nullptr)
    {
      return false;
    }

    const AABB& localBounds = model->getLocalBounds();
    if (!localBounds.isValid())
    {
      // No valid bounds - assume visible
      return true;
    }

    AABB worldBounds = transformAABB(localBounds, modelMatrix);
    return aabbInFrustum(worldBounds, frustum);
  }

  namespace {
    constexpr float kFeatureEpsCpu = 0.01f;

    const PBRMaterial* resolveMaterialForSubMesh(FrameInfo& frameInfo, entt::entity entity, const ModelComponent& modelComp, const Model::SubMesh& subMesh)
    {
      if (auto* mat = frameInfo.scene->getRegistry().try_get<PBRMaterial>(entity))
      {
        return mat;
      }

      const auto& materials = modelComp.model->getMaterials();
      if (subMesh.materialId >= 0 && subMesh.materialId < static_cast<int>(materials.size()))
      {
        return &materials[subMesh.materialId].pbrMaterial;
      }

      return nullptr;
    }

    MeshPushConstantData makeMeshPush(FrameInfo const&      frameInfo,
                                      entt::entity          entity,
                                      const ModelComponent& modelComp,
                                      const Model::SubMesh& subMesh,
                                      const glm::mat4&      modelMatrix,
                                      const PBRMaterial*    pMaterial,
                                      bool                  doubleSided,
                                      bool                  isTransparent = false,
                                      bool                  skipHZB       = false)
    {
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
      // Bit 0: double-sided (skip cone culling)
      // Bit 1: transparent (skip cone culling - back faces may be visible)
      // Bit 2: skip HZB occlusion culling (used for depth prepass)
      push.cullingFlags = (doubleSided ? 1u : 0u) | (isTransparent ? 2u : 0u) | (skipHZB ? 4u : 0u);

      // Populate per-instance lightmap transform (if present on the entity)
      push.lightmapUvScale  = glm::vec2(1.0f, 1.0f);
      push.lightmapUvOffset = glm::vec2(0.0f, 0.0f);
      push.lightmapIndex    = 0u;

      auto& reg = frameInfo.scene->getRegistry();
      if (reg.all_of<engine::LightmapComponent>(entity))
      {
        auto& lm              = reg.get<engine::LightmapComponent>(entity);
        push.lightmapUvScale  = lm.uvScale;
        push.lightmapUvOffset = lm.uvOffset;
        if (lm.textureIndex >= 0)
        {
          push.lightmapIndex = static_cast<uint32_t>(lm.textureIndex);
        }
      }

      // If material has a bound lightmap texture, prefer its global index
      if (pMaterial != nullptr && pMaterial->lightmap)
      {
        push.lightmapIndex = pMaterial->lightmap->getGlobalIndex();
      }

      return push;
    }

    void pushConstantsAndDraw(Device& device, VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout, const MeshPushConstantData& push, uint32_t meshletCount)
    {
      vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_TASK_BIT_EXT | VK_SHADER_STAGE_MESH_BIT_EXT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(MeshPushConstantData), &push);

      if (device.vkCmdDrawMeshTasksEXT != nullptr)
      {
        uint32_t const groupCount = (meshletCount + 31) / 32;
        device.vkCmdDrawMeshTasksEXT(commandBuffer, groupCount, 1, 1);
      }
    }
  } // namespace

  ModelRenderSystem::ModelRenderSystem(Device& device, VkRenderPass renderPass, VkDescriptorSetLayout globalSetLayout, VkDescriptorSetLayout bindlessSetLayout) : device(device)
  {
    createSceneColorDescriptorResources();

    lightingBindings_ = std::make_unique<LightingRenderBindings>(device);
    lightingBindings_->createResources();

    materialBindings_ = std::make_unique<MaterialRenderBindings>(device);
    materialBindings_->createResources();

    createPipelineLayout(globalSetLayout, bindlessSetLayout);
    createPipeline(renderPass);
  }

  ModelRenderSystem::~ModelRenderSystem()
  {
    vkDestroyPipelineLayout(device.device(), pipelineLayout, nullptr);

    if (sceneColorDescriptorSetLayout_ != VK_NULL_HANDLE)
    {
      vkDestroyDescriptorSetLayout(device.device(), sceneColorDescriptorSetLayout_, nullptr);
    }
  }

  void ModelRenderSystem::createSceneColorDescriptorResources()
  {
    VkDescriptorSetLayoutBinding binding{};
    binding.binding            = 0;
    binding.descriptorType     = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    binding.descriptorCount    = 1;
    binding.stageFlags         = VK_SHADER_STAGE_FRAGMENT_BIT;
    binding.pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings    = &binding;

    if (vkCreateDescriptorSetLayout(device.device(), &layoutInfo, nullptr, &sceneColorDescriptorSetLayout_) != VK_SUCCESS)
    {
      throw std::runtime_error("Failed to create scene color descriptor set layout");
    }

    const uint32_t count = static_cast<uint32_t>(SwapChain::maxFramesInFlight());

    sceneColorDescriptorPool_ = engine::DescriptorPool::Builder(device).setMaxSets(count).addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, count).build();

    sceneColorDescriptorSets_.resize(count);
    for (uint32_t i = 0; i < count; ++i)
    {
      if (!sceneColorDescriptorPool_->allocateDescriptor(sceneColorDescriptorSetLayout_, sceneColorDescriptorSets_[i]))
      {
        throw std::runtime_error("Failed to allocate scene color descriptor sets");
      }
    }
  }

  void ModelRenderSystem::createPipelineLayout(VkDescriptorSetLayout globalSetLayout, VkDescriptorSetLayout bindlessSetLayout)
  {
    VkPushConstantRange const pushConstantRange{
            .stageFlags = VK_SHADER_STAGE_TASK_BIT_EXT | VK_SHADER_STAGE_MESH_BIT_EXT | VK_SHADER_STAGE_FRAGMENT_BIT,
            .offset     = 0,
            .size       = sizeof(MeshPushConstantData),
    };

    std::vector<VkDescriptorSetLayout> descriptorSetLayouts{globalSetLayout,
                                                            bindlessSetLayout,
                                                            lightingBindings_ ? lightingBindings_->getShadowDescriptorSetLayout() : VK_NULL_HANDLE,
                                                            lightingBindings_ ? lightingBindings_->getIBLDescriptorSetLayout() : VK_NULL_HANDLE,
                                                            materialBindings_ ? materialBindings_->getDescriptorSetLayout() : VK_NULL_HANDLE,
                                                            sceneColorDescriptorSetLayout_};

    VkPipelineLayoutCreateInfo const pipelineLayoutInfo{
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

  void ModelRenderSystem::createPipeline(VkRenderPass renderPass)
  {
    assert(pipelineLayout != VK_NULL_HANDLE && "Pipeline layout must be created before pipeline.");

    PipelineConfigInfo pipelineConfig{};
    Pipeline::defaultMeshPipelineConfigInfo(pipelineConfig);

    // When a depth prepass is used, the main shading pass will often see equal depth values.
    // Using LESS here would reject those fragments, making opaque objects disappear or look wrong.
    pipelineConfig.depthStencilInfo.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;

    pipelineConfig.renderPass     = renderPass;
    pipelineConfig.pipelineLayout = pipelineLayout;

    // Create Transparent Pipeline (alpha blend compositor)
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
                                                     std::string(SHADER_PATH) + R"(simple_mesh.task.spv)",
                                                     std::string(SHADER_PATH) + R"(simple_mesh.mesh.spv)",
                                                     std::string(SHADER_PATH) + R"(pbr_shader.frag.spv)",
                                                     transparentConfig);

    standardTransparentPipeline = std::make_unique<Pipeline>(device,
                                                             std::string(SHADER_PATH) + R"(simple_mesh.task.spv)",
                                                             std::string(SHADER_PATH) + R"(simple_mesh.mesh.spv)",
                                                             std::string(SHADER_PATH) + R"(pbr_shader_standard.frag.spv)",
                                                             transparentConfig);

    // Create Transmission Pipeline (no blending, no depth write; shaded refraction)
    PipelineConfigInfo transmissionConfig                = pipelineConfig;
    transmissionConfig.colorBlendAttachment.blendEnable  = VK_FALSE;
    transmissionConfig.colorBlendInfo.pAttachments       = &transmissionConfig.colorBlendAttachment;
    transmissionConfig.depthStencilInfo.depthWriteEnable = VK_FALSE;

    transmissionPipeline = std::make_unique<Pipeline>(device,
                                                      std::string(SHADER_PATH) + R"(simple_mesh.task.spv)",
                                                      std::string(SHADER_PATH) + R"(simple_mesh.mesh.spv)",
                                                      std::string(SHADER_PATH) + R"(pbr_shader.frag.spv)",
                                                      transmissionConfig);

    standardTransmissionPipeline = std::make_unique<Pipeline>(device,
                                                              std::string(SHADER_PATH) + R"(simple_mesh.task.spv)",
                                                              std::string(SHADER_PATH) + R"(simple_mesh.mesh.spv)",
                                                              std::string(SHADER_PATH) + R"(pbr_shader_standard.frag.spv)",
                                                              transmissionConfig);
  }

  void ModelRenderSystem::bindBaseDescriptorSets(FrameInfo& frameInfo, bool bindSceneColor) const
  {
    vkCmdBindDescriptorSets(frameInfo.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &frameInfo.globalDescriptorSet, 0, nullptr);
    vkCmdBindDescriptorSets(frameInfo.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 1, 1, &frameInfo.globalTextureSet, 0, nullptr);
    if (bindSceneColor && !sceneColorDescriptorSets_.empty())
    {
      vkCmdBindDescriptorSets(frameInfo.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 5, 1, &sceneColorDescriptorSets_[frameInfo.frameIndex], 0, nullptr);
    }
  }

  void ModelRenderSystem::createGbufferPipeline(VkRenderPass renderPass)
  {
    assert(pipelineLayout != VK_NULL_HANDLE && "Pipeline layout must be created before pipeline.");

    PipelineConfigInfo pipelineConfig{};
    Pipeline::defaultMeshPipelineConfigInfo(pipelineConfig);

    // Depth compare matches the main mesh pipeline behavior when a depth prepass is used.
    pipelineConfig.depthStencilInfo.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;

    // G-buffer has 4 MRTs (N, Albedo, Material, HDR emissive); disable blending for all.
    std::array<VkPipelineColorBlendAttachmentState, 4> attachments{};
    for (auto& a : attachments)
    {
      a             = pipelineConfig.colorBlendAttachment;
      a.blendEnable = VK_FALSE;
    }
    pipelineConfig.colorBlendInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    pipelineConfig.colorBlendInfo.pAttachments    = attachments.data();

    pipelineConfig.renderPass     = renderPass;
    pipelineConfig.pipelineLayout = pipelineLayout;

    gbufferPipeline = std::make_unique<Pipeline>(device,
                                                 std::string(SHADER_PATH) + R"(simple_mesh.task.spv)",
                                                 std::string(SHADER_PATH) + R"(simple_mesh.mesh.spv)",
                                                 std::string(SHADER_PATH) + R"(gbuffer.frag.spv)",
                                                 pipelineConfig);
  }

  void ModelRenderSystem::renderGbuffer(FrameInfo& frameInfo)
  {
    if (!gbufferPipeline)
    {
      // G-buffer not enabled; no-op.
      return;
    }

    // Extract frustum for CPU culling
    glm::mat4 const vp      = frameInfo.camera.getProjection() * frameInfo.camera.getView();
    Frustum const   frustum = extractFrustumFromMatrix(vp);

    auto view = frameInfo.scene->getRegistry().view<ModelComponent, TransformComponent>();

    Pipeline* boundPipeline        = nullptr;
    auto      bindPipelineIfNeeded = [&](Pipeline* p) {
      if (p != nullptr && boundPipeline != p)
      {
        p->bind(frameInfo.commandBuffer);
        boundPipeline = p;
      }
    };

    // Bind pipeline + common descriptor sets once.
    bindPipelineIfNeeded(gbufferPipeline.get());
    bindBaseDescriptorSets(frameInfo, true);

    auto renderItem = [&](entt::entity entity, const Model::SubMesh& subMesh, const PBRMaterial* pMaterial, const glm::mat4& modelMatrix) {
      auto&                      modelComp = view.get<ModelComponent>(entity);
      MeshPushConstantData const push      = makeMeshPush(frameInfo, entity, modelComp, subMesh, modelMatrix, pMaterial, (pMaterial != nullptr) && pMaterial->doubleSided);

      float const isSelected = ((uint32_t)entity == frameInfo.selectedObjectId) ? 1.0f : 0.0f;
      if (materialBindings_ != nullptr)
      {
        materialBindings_->bindMaterial(frameInfo, pipelineLayout, pMaterial, isSelected);
      }

      pushConstantsAndDraw(device, frameInfo.commandBuffer, pipelineLayout, push, subMesh.meshletCount);
    };

    for (auto entity : view)
    {
      auto [modelComp, transform] = view.get<ModelComponent, TransformComponent>(entity);
      if (!modelComp.model)
      {
        continue;
      }

      // CPU frustum culling: skip entire object if outside view
      glm::mat4 const modelMatrix = transform.modelTransform();
      if (!isEntityVisible(modelComp.model.get(), modelMatrix, frustum))
      {
        continue;
      }

      const auto& subMeshes = modelComp.model->getSubMeshes();

      for (const auto& subMesh : subMeshes)
      {
        if (subMesh.meshletCount == 0)
        {
          continue;
        }

        const PBRMaterial* pMaterial = resolveMaterialForSubMesh(frameInfo, entity, modelComp, subMesh);

        bool hasTransmission = false;
        bool isAlphaBlend    = false;
        if (pMaterial != nullptr)
        {
          hasTransmission = (pMaterial->transmission > kFeatureEpsCpu) || pMaterial->hasTransmissionMap();
          isAlphaBlend    = (pMaterial->alphaMode == AlphaMode::Blend);
        }

        if (isAlphaBlend || hasTransmission)
        {
          continue;
        }

        bindPipelineIfNeeded(gbufferPipeline.get());
        renderItem(entity, subMesh, pMaterial, transform.modelTransform());
      }
    }
  }

  void ModelRenderSystem::beginFrame(int frameIndex)
  {
    if (materialBindings_ != nullptr)
    {
      materialBindings_->beginFrame(frameIndex);
    }
  }

  void ModelRenderSystem::updateSceneColorDescriptor(int frameIndex, VkDescriptorImageInfo const& sceneColorInfo)
  {
    if (frameIndex < 0 || frameIndex >= static_cast<int>(sceneColorDescriptorSets_.size()))
    {
      return;
    }

    VkWriteDescriptorSet write{};
    write.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet          = sceneColorDescriptorSets_[frameIndex];
    write.dstBinding      = 0;
    write.dstArrayElement = 0;
    write.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.descriptorCount = 1;
    write.pImageInfo      = &sceneColorInfo;

    vkUpdateDescriptorSets(device.device(), 1, &write, 0, nullptr);
  }

  void ModelRenderSystem::createDepthPrepassPipeline(VkRenderPass renderPass)
  {
    PipelineConfigInfo prepassConfig{};
    Pipeline::defaultMeshPipelineConfigInfo(prepassConfig);

    // Depth-only: no color attachments in the render pass.
    prepassConfig.colorBlendInfo.attachmentCount = 0;
    prepassConfig.colorBlendInfo.pAttachments    = nullptr;

    prepassConfig.depthStencilInfo.depthTestEnable  = VK_TRUE;
    prepassConfig.depthStencilInfo.depthWriteEnable = VK_TRUE;
    prepassConfig.depthStencilInfo.depthCompareOp   = VK_COMPARE_OP_LESS_OR_EQUAL;

    prepassConfig.renderPass     = renderPass;
    prepassConfig.pipelineLayout = pipelineLayout;

    depthPrepassPipeline = std::make_unique<Pipeline>(device,
                                                      std::string(SHADER_PATH) + R"(simple_mesh.task.spv)",
                                                      std::string(SHADER_PATH) + R"(simple_mesh.mesh.spv)",
                                                      std::string(SHADER_PATH) + R"(depth_only.frag.spv)",
                                                      prepassConfig);
  }

  void ModelRenderSystem::setShadowSystem(ShadowSystem* shadowSystem)
  {
    if (lightingBindings_ != nullptr)
    {
      lightingBindings_->setShadowSystem(shadowSystem);
    }
  }

  void ModelRenderSystem::setIBLSystem(IBLSystem* iblSystem)
  {
    if (lightingBindings_ != nullptr)
    {
      lightingBindings_->setIBLSystem(iblSystem);
    }
  }

  void ModelRenderSystem::renderTransmission(FrameInfo& frameInfo)
  {
    // Extract frustum for CPU culling
    glm::mat4 const vp      = frameInfo.camera.getProjection() * frameInfo.camera.getView();
    Frustum const   frustum = extractFrustumFromMatrix(vp);

    Pipeline* boundPipeline        = nullptr;
    auto      bindPipelineIfNeeded = [&](Pipeline* p) {
      if (p != nullptr && boundPipeline != p)
      {
        p->bind(frameInfo.commandBuffer);
        boundPipeline = p;
      }
    };

    bindPipelineIfNeeded(standardTransmissionPipeline.get());
    bindBaseDescriptorSets(frameInfo, true);
    if (lightingBindings_ != nullptr)
    {
      lightingBindings_->bindShadow(frameInfo, pipelineLayout);
      lightingBindings_->bindIBL(frameInfo, pipelineLayout);
    }

    auto view = frameInfo.scene->getRegistry().view<ModelComponent, TransformComponent>();

    auto renderItem = [&](entt::entity entity, const Model::SubMesh& subMesh, const PBRMaterial* pMaterial, const glm::mat4& modelMatrix) {
      auto& modelComp = view.get<ModelComponent>(entity);

      MeshPushConstantData const push = makeMeshPush(frameInfo, entity, modelComp, subMesh, modelMatrix, pMaterial, (pMaterial != nullptr) && pMaterial->doubleSided, true /* isTransparent */);

      float const isSelected = ((uint32_t)entity == frameInfo.selectedObjectId) ? 1.0f : 0.0f;
      if (materialBindings_ != nullptr)
      {
        materialBindings_->bindMaterial(frameInfo, pipelineLayout, pMaterial, isSelected);
      }

      pushConstantsAndDraw(device, frameInfo.commandBuffer, pipelineLayout, push, subMesh.meshletCount);
    };

    for (auto entity : view)
    {
      auto [modelComp, transform] = view.get<ModelComponent, TransformComponent>(entity);
      if (!modelComp.model)
      {
        continue;
      }

      // CPU frustum culling: skip entire object if outside view
      glm::mat4 const modelMatrix = transform.modelTransform();
      if (!isEntityVisible(modelComp.model.get(), modelMatrix, frustum))
      {
        continue;
      }

      const auto& subMeshes = modelComp.model->getSubMeshes();

      for (const auto& subMesh : subMeshes)
      {
        if (subMesh.meshletCount == 0)
        {
          continue;
        }

        const PBRMaterial* pMaterial = resolveMaterialForSubMesh(frameInfo, entity, modelComp, subMesh);

        bool hasTransmission = false;
        if (pMaterial != nullptr)
        {
          hasTransmission = (pMaterial->transmission > kFeatureEpsCpu) || pMaterial->hasTransmissionMap();
        }

        if (!hasTransmission)
        {
          continue;
        }

        Pipeline* desired = MaterialRenderBindings::needsFullVariant(frameInfo, pMaterial) ? transmissionPipeline.get() : standardTransmissionPipeline.get();
        bindPipelineIfNeeded(desired);
        renderItem(entity, subMesh, pMaterial, transform.modelTransform());
      }
    }
  }

  void ModelRenderSystem::renderAlphaBlend(FrameInfo& frameInfo)
  {
    // Extract frustum for CPU culling
    glm::mat4 const vp      = frameInfo.camera.getProjection() * frameInfo.camera.getView();
    Frustum const   frustum = extractFrustumFromMatrix(vp);

    Pipeline* boundPipeline        = nullptr;
    auto      bindPipelineIfNeeded = [&](Pipeline* p) {
      if (p != nullptr && boundPipeline != p)
      {
        p->bind(frameInfo.commandBuffer);
        boundPipeline = p;
      }
    };

    bindPipelineIfNeeded(standardTransparentPipeline.get());
    bindBaseDescriptorSets(frameInfo, true);
    if (lightingBindings_ != nullptr)
    {
      lightingBindings_->bindShadow(frameInfo, pipelineLayout);
      lightingBindings_->bindIBL(frameInfo, pipelineLayout);
    }

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

    auto renderItem = [&](entt::entity entity, const Model::SubMesh& subMesh, const PBRMaterial* pMaterial, const glm::mat4& modelMatrix) {
      auto& modelComp = view.get<ModelComponent>(entity);

      MeshPushConstantData const push = makeMeshPush(frameInfo, entity, modelComp, subMesh, modelMatrix, pMaterial, (pMaterial != nullptr) && pMaterial->doubleSided, true /* isTransparent */);

      float const isSelected = ((uint32_t)entity == frameInfo.selectedObjectId) ? 1.0f : 0.0f;
      if (materialBindings_ != nullptr)
      {
        materialBindings_->bindMaterial(frameInfo, pipelineLayout, pMaterial, isSelected);
      }

      pushConstantsAndDraw(device, frameInfo.commandBuffer, pipelineLayout, push, subMesh.meshletCount);
    };

    for (auto entity : view)
    {
      auto [modelComp, transform] = view.get<ModelComponent, TransformComponent>(entity);
      if (!modelComp.model)
      {
        continue;
      }

      // CPU frustum culling: skip entire object if outside view
      glm::mat4 const modelMatrix = transform.modelTransform();
      if (!isEntityVisible(modelComp.model.get(), modelMatrix, frustum))
      {
        continue;
      }

      const auto& subMeshes = modelComp.model->getSubMeshes();

      for (const auto& subMesh : subMeshes)
      {
        if (subMesh.meshletCount == 0)
        {
          continue;
        }

        const PBRMaterial* pMaterial = resolveMaterialForSubMesh(frameInfo, entity, modelComp, subMesh);

        bool hasTransmission = false;
        bool isAlphaBlend    = false;
        if (pMaterial != nullptr)
        {
          hasTransmission = (pMaterial->transmission > kFeatureEpsCpu) || pMaterial->hasTransmissionMap();
          isAlphaBlend    = (pMaterial->alphaMode == AlphaMode::Blend);
        }

        if (!isAlphaBlend || hasTransmission)
        {
          continue;
        }

        glm::vec3 const worldPos = glm::vec3(transform.modelTransform()[3]);
        float const     dist     = glm::distance(worldPos, frameInfo.camera.getPosition());
        transparentItems.push_back({entity, &subMesh, pMaterial, transform.modelTransform(), dist});
      }
    }

    std::sort(transparentItems.begin(), transparentItems.end(), [](const TransparentRenderItem& a, const TransparentRenderItem& b) { return a.distance > b.distance; });

    boundPipeline = nullptr;
    for (const auto& item : transparentItems)
    {
      Pipeline* desired = MaterialRenderBindings::needsFullVariant(frameInfo, item.material) ? transparentPipeline.get() : standardTransparentPipeline.get();
      bindPipelineIfNeeded(desired);
      renderItem(item.entity, *item.subMesh, item.material, item.modelMatrix);
    }
  }

  void ModelRenderSystem::renderDepthPrepass(FrameInfo& frameInfo)
  {
    if (!depthPrepassPipeline)
    {
      // Depth prepass is optional until wired into RenderGraph.
      return;
    }

    // Extract frustum for CPU culling
    glm::mat4 const vp      = frameInfo.camera.getProjection() * frameInfo.camera.getView();
    Frustum const   frustum = extractFrustumFromMatrix(vp);

    Pipeline* boundPipeline        = nullptr;
    auto      bindPipelineIfNeeded = [&](Pipeline* p) {
      if (p != nullptr && boundPipeline != p)
      {
        p->bind(frameInfo.commandBuffer);
        boundPipeline = p;
      }
    };

    bindPipelineIfNeeded(depthPrepassPipeline.get());

    vkCmdBindDescriptorSets(frameInfo.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &frameInfo.globalDescriptorSet, 0, nullptr);
    vkCmdBindDescriptorSets(frameInfo.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 1, 1, &frameInfo.globalTextureSet, 0, nullptr);

    // Bind lighting sets (pipeline layout expects it, even if not used by the depth-only fragment shader)
    if (lightingBindings_ != nullptr)
    {
      lightingBindings_->bindShadow(frameInfo, pipelineLayout);
      lightingBindings_->bindIBL(frameInfo, pipelineLayout);
    }

    auto view = frameInfo.scene->getRegistry().view<ModelComponent, TransformComponent>();

    auto renderItem = [&](entt::entity entity, const Model::SubMesh& subMesh, const PBRMaterial* pMaterial, const glm::mat4& modelMatrix) {
      auto& modelComp = view.get<ModelComponent>(entity);

      // Depth prepass skips HZB culling (bit 2) to ensure complete depth buffer for HZB generation
      MeshPushConstantData const push = makeMeshPush(frameInfo, entity, modelComp, subMesh, modelMatrix, nullptr, false, true);

      // Populate a default material record so the dynamic UBO binding is always valid.
      if (materialBindings_ != nullptr)
      {
        materialBindings_->bindMaterial(frameInfo, pipelineLayout, nullptr, 0.0f);
      }

      pushConstantsAndDraw(device, frameInfo.commandBuffer, pipelineLayout, push, subMesh.meshletCount);
    };

    // Render opaque-only; skip masked/transparent materials for now.
    for (auto entity : view)
    {
      auto [modelComp, transform] = view.get<ModelComponent, TransformComponent>(entity);
      if (!modelComp.model)
      {
        continue;
      }

      // CPU frustum culling: skip entire object if outside view
      glm::mat4 const modelMatrix = transform.modelTransform();
      if (!isEntityVisible(modelComp.model.get(), modelMatrix, frustum))
      {
        continue;
      }

      const auto& subMeshes = modelComp.model->getSubMeshes();

      for (const auto& subMesh : subMeshes)
      {
        if (subMesh.meshletCount == 0)
        {
          continue;
        }

        const PBRMaterial* pMaterial = resolveMaterialForSubMesh(frameInfo, entity, modelComp, subMesh);

        bool prepassEligible = true;
        if (pMaterial != nullptr)
        {
          if (pMaterial->alphaMode != AlphaMode::Opaque)
          {
            prepassEligible = false;
          }
          if (pMaterial->transmission > 0.0f)
          {
            prepassEligible = false;
          }
        }

        if (!prepassEligible)
        {
          continue;
        }

        bindPipelineIfNeeded(depthPrepassPipeline.get());
        renderItem(entity, subMesh, pMaterial, transform.modelTransform());
      }
    }
  }
} // namespace engine
