#include "Engine/Systems/ModelRenderSystem.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "Engine/Core/Exceptions.hpp"
#include "Engine/Graphics/Device.hpp"
#include "Engine/Graphics/FrameInfo.hpp"
#include "Engine/Graphics/Pipeline.hpp"
#include "Engine/Graphics/SwapChain.hpp"
#include "Engine/Resources/Model.hpp"
#include "Engine/Resources/PBRMaterial.hpp"
#include "Engine/Scene/components/ModelComponent.hpp"
#include "Engine/Scene/components/TransformComponent.hpp"
#include "Engine/Systems/LightingRenderBindings.hpp"
#include "Engine/Systems/MaterialRenderBindings.hpp"
#include "entt/entity/fwd.hpp"
#include "glm/ext/matrix_float4x4.hpp"
#include "glm/ext/vector_float2.hpp"
#include "glm/ext/vector_float3.hpp"
#include "glm/geometric.hpp"
#include "glm/matrix.hpp"
#include "vulkan/vulkan_core.h"

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

    MeshPushConstantData makeMeshPush(FrameInfo const& frameInfo, const ModelComponent& modelComp, const Model::SubMesh& subMesh, const glm::mat4& modelMatrix, bool doubleSided)
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
      push.cullingFlags            = doubleSided ? 1u : 0u;
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

    if (sceneColorDescriptorPool_ != VK_NULL_HANDLE)
    {
      vkDestroyDescriptorPool(device.device(), sceneColorDescriptorPool_, nullptr);
    }
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

    VkDescriptorPoolSize poolSize{};
    poolSize.type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSize.descriptorCount = static_cast<uint32_t>(SwapChain::maxFramesInFlight());

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes    = &poolSize;
    poolInfo.maxSets       = static_cast<uint32_t>(SwapChain::maxFramesInFlight());

    if (vkCreateDescriptorPool(device.device(), &poolInfo, nullptr, &sceneColorDescriptorPool_) != VK_SUCCESS)
    {
      throw std::runtime_error("Failed to create scene color descriptor pool");
    }

    std::vector<VkDescriptorSetLayout> layouts(SwapChain::maxFramesInFlight(), sceneColorDescriptorSetLayout_);

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool     = sceneColorDescriptorPool_;
    allocInfo.descriptorSetCount = static_cast<uint32_t>(SwapChain::maxFramesInFlight());
    allocInfo.pSetLayouts        = layouts.data();

    sceneColorDescriptorSets_.resize(SwapChain::maxFramesInFlight());
    if (vkAllocateDescriptorSets(device.device(), &allocInfo, sceneColorDescriptorSets_.data()) != VK_SUCCESS)
    {
      throw std::runtime_error("Failed to allocate scene color descriptor sets");
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
      MeshPushConstantData const push      = makeMeshPush(frameInfo, modelComp, subMesh, modelMatrix, (pMaterial != nullptr) && pMaterial->doubleSided);

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

      MeshPushConstantData const push = makeMeshPush(frameInfo, modelComp, subMesh, modelMatrix, (pMaterial != nullptr) && pMaterial->doubleSided);

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

      MeshPushConstantData const push = makeMeshPush(frameInfo, modelComp, subMesh, modelMatrix, (pMaterial != nullptr) && pMaterial->doubleSided);

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

      MeshPushConstantData const push = makeMeshPush(frameInfo, modelComp, subMesh, modelMatrix, (pMaterial != nullptr) && pMaterial->doubleSided);

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
