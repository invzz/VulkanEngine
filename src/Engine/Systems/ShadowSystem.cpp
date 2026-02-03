#include "Engine/Systems/ShadowSystem.hpp"

#include <cstdint>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>

#include "Engine/Core/ansi_colors.hpp"
#include "Engine/Graphics/CubeShadowMap.hpp"
#include "Engine/Graphics/Device.hpp"
#include "Engine/Graphics/FrameInfo.hpp"
#include "Engine/Graphics/Pipeline.hpp"
#include "Engine/Graphics/ShadowMap.hpp"
#include "ModelLib/Resources/Model.hpp"
#include "ModelLib/Resources/PBRMaterial.hpp"
#include "Engine/Scene/components/DirectionalLightComponent.hpp"
#include "Engine/Scene/components/ModelComponent.hpp"
#include "Engine/Scene/components/PointLightComponent.hpp"
#include "Engine/Scene/components/SpotLightComponent.hpp"
#include "Engine/Scene/components/TransformComponent.hpp"
#include "glm/common.hpp"
#include "glm/ext/matrix_clip_space.hpp"
#include "glm/ext/matrix_float4x4.hpp"
#include "glm/ext/matrix_transform.hpp"
#include "glm/ext/vector_float4.hpp"
#include "glm/geometric.hpp"
#include "glm/trigonometric.hpp"
#include "vulkan/vulkan_core.h"

namespace engine {

  // ─────────────────────────────────────────────────────────────────────────────
  // Push constants for mesh shader shadow rendering (Level 3)
  // ─────────────────────────────────────────────────────────────────────────────

  struct ShadowMeshPushConstants
  {
    glm::mat4 modelMatrix;
    glm::mat4 lightSpaceMatrix;

    uint64_t meshletBufferAddress;
    uint64_t meshletVerticesAddress;
    uint64_t meshletTrianglesAddress;
    uint64_t vertexBufferAddress;
    uint32_t meshletOffset;
    uint32_t meshletCount;
  };

  struct CubeShadowMeshPushConstants
  {
    glm::mat4 modelMatrix;
    glm::mat4 lightSpaceMatrix;
    glm::vec4 lightPosAndFarPlane; // xyz = light position, w = far plane

    uint64_t meshletBufferAddress;
    uint64_t meshletVerticesAddress;
    uint64_t meshletTrianglesAddress;
    uint64_t vertexBufferAddress;
    uint32_t meshletOffset;
    uint32_t meshletCount;
  };

  // ─────────────────────────────────────────────────────────────────────────────

  ShadowSystem::ShadowSystem(Device& device, uint32_t shadowMapSize) : device_{device}, shadowMapSize_{shadowMapSize}
  {
    // Create multiple shadow maps for directional/spot lights
    for (auto& lightSpaceMatrix : lightSpaceMatrices_)
    {
      shadowMaps_.push_back(std::make_unique<ShadowMap>(device, shadowMapSize, shadowMapSize));
      lightSpaceMatrix = glm::mat4(1.0f);
    }

    // Create cube shadow maps for point lights
    for (int i = 0; i < MAX_CUBE_SHADOW_MAPS; i++)
    {
      cubeShadowMaps_.push_back(std::make_unique<CubeShadowMap>(device, shadowMapSize));
      pointLightPositions_[i] = glm::vec3(0.0f);
      pointLightRanges_[i]    = 25.0f;
    }

    createMeshPipelineLayout();
    createMeshPipeline();
    createCubeMeshPipelineLayout();
    createCubeMeshPipeline();

    std::cout << "[" << GREEN << "ShadowSystem" << RESET << "] Initialized with " << MAX_SHADOW_MAPS << " 2D shadow maps and " << MAX_CUBE_SHADOW_MAPS << " cube shadow maps (" << shadowMapSize << "x"
              << shadowMapSize << "), mesh shader culling (Level 3)\n";
  }

  ShadowSystem::~ShadowSystem()
  {
    if (meshPipelineLayout_ != VK_NULL_HANDLE)
    {
      vkDestroyPipelineLayout(device_.device(), meshPipelineLayout_, nullptr);
    }
    if (cubeMeshPipelineLayout_ != VK_NULL_HANDLE)
    {
      vkDestroyPipelineLayout(device_.device(), cubeMeshPipelineLayout_, nullptr);
    }
  }

  void ShadowSystem::createMeshPipelineLayout()
  {
    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_TASK_BIT_EXT | VK_SHADER_STAGE_MESH_BIT_EXT;
    pushConstantRange.offset     = 0;
    pushConstantRange.size       = sizeof(ShadowMeshPushConstants);

    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount         = 0;
    layoutInfo.pSetLayouts            = nullptr;
    layoutInfo.pushConstantRangeCount = 1;
    layoutInfo.pPushConstantRanges    = &pushConstantRange;

    if (vkCreatePipelineLayout(device_.device(), &layoutInfo, nullptr, &meshPipelineLayout_) != VK_SUCCESS)
    {
      throw std::runtime_error("Failed to create shadow mesh pipeline layout");
    }
  }

  void ShadowSystem::createMeshPipeline()
  {
    PipelineConfigInfo configInfo{};
    Pipeline::defaultMeshPipelineConfigInfo(configInfo);

    // No color attachment - depth only
    configInfo.colorBlendInfo.attachmentCount = 0;
    configInfo.colorBlendAttachment           = {};

    // Depth bias to prevent shadow acne
    configInfo.rasterizationInfo.depthBiasEnable         = VK_TRUE;
    configInfo.rasterizationInfo.depthBiasConstantFactor = 1.25f;
    configInfo.rasterizationInfo.depthBiasSlopeFactor    = 1.75f;

    // No culling for shadows (all geometry matters)
    configInfo.rasterizationInfo.cullMode = VK_CULL_MODE_NONE;

    configInfo.renderPass     = shadowMaps_[0]->getRenderPass();
    configInfo.pipelineLayout = meshPipelineLayout_;

    meshPipeline_ = std::make_unique<Pipeline>(device_,
                                               std::string(SHADER_PATH) + R"(shadow.task.spv)",
                                               std::string(SHADER_PATH) + R"(shadow.mesh.spv)",
                                               std::string(SHADER_PATH) + R"(shadow.frag.spv)",
                                               configInfo);
  }

  void ShadowSystem::createCubeMeshPipelineLayout()
  {
    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_TASK_BIT_EXT | VK_SHADER_STAGE_MESH_BIT_EXT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pushConstantRange.offset     = 0;
    pushConstantRange.size       = sizeof(CubeShadowMeshPushConstants);

    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount         = 0;
    layoutInfo.pSetLayouts            = nullptr;
    layoutInfo.pushConstantRangeCount = 1;
    layoutInfo.pPushConstantRanges    = &pushConstantRange;

    if (vkCreatePipelineLayout(device_.device(), &layoutInfo, nullptr, &cubeMeshPipelineLayout_) != VK_SUCCESS)
    {
      throw std::runtime_error("Failed to create cube shadow mesh pipeline layout");
    }
  }

  void ShadowSystem::createCubeMeshPipeline()
  {
    PipelineConfigInfo configInfo{};
    Pipeline::defaultMeshPipelineConfigInfo(configInfo);

    // No color attachment - depth only
    configInfo.colorBlendInfo.attachmentCount = 0;
    configInfo.colorBlendAttachment           = {};

    // Depth bias to prevent shadow acne
    configInfo.rasterizationInfo.depthBiasEnable         = VK_TRUE;
    configInfo.rasterizationInfo.depthBiasConstantFactor = 1.25f;
    configInfo.rasterizationInfo.depthBiasSlopeFactor    = 1.75f;

    // No culling for point light shadows
    configInfo.rasterizationInfo.cullMode = VK_CULL_MODE_NONE;

    configInfo.renderPass     = cubeShadowMaps_[0]->getRenderPass();
    configInfo.pipelineLayout = cubeMeshPipelineLayout_;

    cubeMeshPipeline_ = std::make_unique<Pipeline>(device_,
                                                   std::string(SHADER_PATH) + R"(cube_shadow.task.spv)",
                                                   std::string(SHADER_PATH) + R"(cube_shadow.mesh.spv)",
                                                   std::string(SHADER_PATH) + R"(cube_shadow_mesh.frag.spv)",
                                                   configInfo);
  }

  glm::mat4 ShadowSystem::calculateDirectionalCascadeMatrix(const glm::vec3& lightDirection,
                                                            const Camera&    camera,
                                                            float            cascadeNear,
                                                            float            cascadeFar,
                                                            int              cascadeIndex,
                                                            uint32_t         shadowMapSize,
                                                            glm::vec3*       outMinLS,
                                                            glm::vec3*       outMaxLS)
  {
    // Normalize light direction
    glm::vec3 const lightDir = glm::normalize(lightDirection);

    // Get camera matrices
    glm::mat4 const proj    = camera.getProjectionMatrix();
    glm::mat4 const invView = camera.getInverseView();

    // Extract near/far planes from projection matrix
    float const A         = proj[2][2];
    float const B         = proj[3][2];
    float       nearPlane = 0.1f;
    if (glm::abs(A) > 1e-6f)
    {
      nearPlane = glm::max(0.001f, -B / A);
    }

    float farPlane = nearPlane + 100.0f;
    if (glm::abs(A - 1.0f) > 1e-6f)
    {
      farPlane = (A * nearPlane) / (A - 1.0f);
    }

    // Extract field of view and aspect ratio
    float const tanHalfFovy = 1.0f / glm::max(proj[1][1], 1e-6f);
    float const aspect      = proj[1][1] / glm::max(proj[0][0], 1e-6f);

    // Clamp cascade distances to valid range
    float const sliceNear = glm::clamp(cascadeNear, nearPlane, farPlane - 0.01f);
    float const sliceFar  = glm::clamp(cascadeFar, sliceNear + 0.01f, farPlane);

    // Calculate frustum dimensions at near and far planes
    float const nearHeight = 2.0f * tanHalfFovy * sliceNear;
    float const nearWidth  = nearHeight * aspect;
    float const farHeight  = 2.0f * tanHalfFovy * sliceFar;
    float const farWidth   = farHeight * aspect;

    // Get camera position and orientation
    glm::vec3 const camPos   = glm::vec3(invView[3]);
    glm::vec3 const camRight = glm::normalize(glm::vec3(invView[0]));
    glm::vec3 const camUp    = glm::normalize(glm::vec3(invView[1]));
    glm::vec3 const camFwd   = glm::normalize(glm::vec3(invView[2]));

    // Calculate frustum corners in world space
    glm::vec3   frustumCorners[8];
    float const nearZ = sliceNear;
    float const farZ  = sliceFar;
    float const nx    = nearWidth * 0.5f;
    float const ny    = nearHeight * 0.5f;
    float const fx    = farWidth * 0.5f;
    float const fy    = farHeight * 0.5f;

    frustumCorners[0] = camPos + camFwd * nearZ - camRight * nx - camUp * ny; // Near bottom left
    frustumCorners[1] = camPos + camFwd * nearZ + camRight * nx - camUp * ny; // Near bottom right
    frustumCorners[2] = camPos + camFwd * nearZ + camRight * nx + camUp * ny; // Near top right
    frustumCorners[3] = camPos + camFwd * nearZ - camRight * nx + camUp * ny; // Near top left
    frustumCorners[4] = camPos + camFwd * farZ - camRight * fx - camUp * fy;  // Far bottom left
    frustumCorners[5] = camPos + camFwd * farZ + camRight * fx - camUp * fy;  // Far bottom right
    frustumCorners[6] = camPos + camFwd * farZ + camRight * fx + camUp * fy;  // Far top right
    frustumCorners[7] = camPos + camFwd * farZ - camRight * fx + camUp * fy;  // Far top left

    // Calculate frustum center
    glm::vec3 frustumCenter{0.0f};
    for (glm::vec3 const& corner : frustumCorners)
    {
      frustumCenter += corner;
    }
    frustumCenter /= 8.0f;

    // Choose up vector for light view matrix
    glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
    if (glm::abs(glm::dot(lightDir, up)) > 0.99f)
    {
      up = glm::vec3(0.0f, 0.0f, 1.0f);
    }

    glm::mat4 const lightView = glm::lookAt(frustumCenter - lightDir * 50.0f, frustumCenter, up);

    // Transform frustum corners to light space and find AABB
    glm::vec3 minLS(std::numeric_limits<float>::infinity());
    glm::vec3 maxLS(-std::numeric_limits<float>::infinity());
    for (glm::vec3 const& cornerWS : frustumCorners)
    {
      glm::vec4 const cornerLS4 = lightView * glm::vec4(cornerWS, 1.0f);
      glm::vec3 const cornerLS  = glm::vec3(cornerLS4);
      minLS                     = glm::min(minLS, cornerLS);
      maxLS                     = glm::max(maxLS, cornerLS);
    }

    // Add some padding to prevent artifacts at edges
    float const paddingScale = 0.1f;  // 10% padding
    float const rangeX       = maxLS.x - minLS.x;
    float const rangeY       = maxLS.y - minLS.y;
    float const paddingX     = rangeX * paddingScale;
    float const paddingY     = rangeY * paddingScale;
    minLS.x -= paddingX;
    maxLS.x += paddingX;
    minLS.y -= paddingY;
    maxLS.y += paddingY;
    minLS.z -= 10.0f;
    maxLS.z += 10.0f;

    // Texel snapping: Round the projection bounds to shadow map texel boundaries
    // This prevents shadow swimming/shimmering when the camera moves
    float const worldUnitsPerTexelX = (maxLS.x - minLS.x) / static_cast<float>(shadowMapSize);
    float const worldUnitsPerTexelY = (maxLS.y - minLS.y) / static_cast<float>(shadowMapSize);

    minLS.x = glm::floor(minLS.x / worldUnitsPerTexelX) * worldUnitsPerTexelX;
    maxLS.x = glm::floor(maxLS.x / worldUnitsPerTexelX) * worldUnitsPerTexelX;
    minLS.y = glm::floor(minLS.y / worldUnitsPerTexelY) * worldUnitsPerTexelY;
    maxLS.y = glm::floor(maxLS.y / worldUnitsPerTexelY) * worldUnitsPerTexelY;

    // Create orthographic projection matrix
    float const orthoNear = -maxLS.z - 10.0f; // Extend near plane
    float const orthoFar  = -minLS.z + 10.0f; // Extend far plane

    glm::mat4 lightProj = glm::orthoZO(minLS.x, maxLS.x, minLS.y, maxLS.y, orthoNear, orthoFar);
    lightProj[1][1] *= -1; // Flip Y for Vulkan

    // Output bounds if requested
    if (outMinLS) *outMinLS = minLS;
    if (outMaxLS) *outMaxLS = maxLS;

    return lightProj * lightView;
  }

  glm::mat4 ShadowSystem::calculateSpotLightMatrix(const glm::vec3& position, const glm::vec3& direction, float outerCutoffDegrees, float range)
  {
    glm::vec3 const lightDir = glm::normalize(direction);

    glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
    if (glm::abs(glm::dot(lightDir, up)) > 0.99f)
    {
      up = glm::vec3(0.0f, 0.0f, 1.0f);
    }

    glm::mat4 const lightView = glm::lookAt(position, position + lightDir, up);

    float const fov       = glm::radians((outerCutoffDegrees * 2.0f) + 5.0f);
    float const nearPlane = 0.1f;
    float const farPlane  = range > 0.0f ? range : 100.0f;

    glm::mat4 lightProj = glm::perspective(fov, 1.0f, nearPlane, farPlane);
    lightProj[1][1] *= -1;

    return lightProj * lightView;
  }

  void ShadowSystem::renderToShadowMapMesh(FrameInfo& frameInfo, ShadowMap& shadowMap, const glm::mat4& lightSpaceMatrix)
  {
    shadowMap.beginRenderPass(frameInfo.commandBuffer);
    meshPipeline_->bind(frameInfo.commandBuffer);

    // Render all shadow-casting objects using mesh shaders (GPU culling built-in)
    auto view = frameInfo.scene->getRegistry().view<ModelComponent, TransformComponent>();
    for (auto entity : view)
    {
      auto [modelComp, transform] = view.get<ModelComponent, TransformComponent>(entity);
      if (!modelComp.model || modelComp.model->getMeshletCount() == 0)
      {
        continue;
      }

      const auto& model = modelComp.model;

      // For each submesh, dispatch mesh shader draws
      for (const auto& subMesh : model->getSubMeshes())
      {
        if (subMesh.meshletCount == 0)
        {
          continue;
        }

        // Skip transparent meshes for directional light shadows
        const auto& materials = model->getMaterials();
        if (subMesh.materialId >= 0 && subMesh.materialId < static_cast<int>(materials.size()))
        {
          const auto& material = materials[subMesh.materialId];
          if (material.pbrMaterial.alphaMode != engine::AlphaMode::Opaque)
          {
            continue; // Skip transparent meshes
          }
        }

        ShadowMeshPushConstants push{};
        push.modelMatrix             = transform.modelTransform();
        push.lightSpaceMatrix        = lightSpaceMatrix;
        push.meshletBufferAddress    = model->getMeshletBufferAddress();
        push.meshletVerticesAddress  = model->getMeshletVerticesAddress();
        push.meshletTrianglesAddress = model->getMeshletTrianglesAddress();
        push.vertexBufferAddress     = model->getVertexBufferAddress();
        push.meshletOffset           = subMesh.meshletOffset;
        push.meshletCount            = subMesh.meshletCount;

        vkCmdPushConstants(frameInfo.commandBuffer, meshPipelineLayout_, VK_SHADER_STAGE_TASK_BIT_EXT | VK_SHADER_STAGE_MESH_BIT_EXT, 0, sizeof(push), &push);

        // Dispatch mesh shader task groups (32 meshlets per group)
        uint32_t const groupCount = (subMesh.meshletCount + 31) / 32;
        device_.vkCmdDrawMeshTasksEXT(frameInfo.commandBuffer, groupCount, 1, 1);
      }
    }

    engine::ShadowMap::endRenderPass(frameInfo.commandBuffer);
  }

  void ShadowSystem::renderShadowMaps(FrameInfo& frameInfo, float shadowDistance)
  {
    shadowLightCount_            = 0;
    directionalCascadeCount_     = 0;
    directionalCascadeBaseIndex_ = 0;

    for (int i = 0; i < DIRECTIONAL_CASCADE_COUNT; i++)
    {
      directionalCascadeSplits_[i] = 0.0f;
    }

    // Render cascaded shadow maps for first directional light
    auto dirView = frameInfo.scene->getRegistry().view<DirectionalLightComponent, TransformComponent>();
    for (auto entity : dirView)
    {
      if (shadowLightCount_ >= MAX_SHADOW_MAPS)
      {
        break;
      }
      auto [dirLight, transform] = dirView.get<DirectionalLightComponent, TransformComponent>(entity);

      glm::vec3 const lightDir = transform.getForwardDir();

      glm::mat4 const proj      = frameInfo.camera.getProjectionMatrix();
      float const     A         = proj[2][2];
      float const     B         = proj[3][2];
      float           nearPlane = 0.1f;
      if (glm::abs(A) > 1e-6f)
      {
        nearPlane = glm::max(0.001f, -B / A);
      }

      float farPlane = nearPlane + 100.0f;
      if (glm::abs(A - 1.0f) > 1e-6f)
      {
        farPlane = (A * nearPlane) / (A - 1.0f);
      }

      float const csmFar = glm::clamp(shadowDistance, nearPlane + 0.5f, farPlane);

      float const lambda = 0.85f; // Balanced logarithmic distribution
      float       splits[DIRECTIONAL_CASCADE_COUNT];
      for (int i = 0; i < DIRECTIONAL_CASCADE_COUNT; i++)
      {
        float const p        = static_cast<float>(i + 1) / static_cast<float>(DIRECTIONAL_CASCADE_COUNT);
        float const logSplit = nearPlane * glm::pow(csmFar / nearPlane, p);
        float const uniSplit = nearPlane + ((csmFar - nearPlane) * p);
        splits[i]            = glm::mix(uniSplit, logSplit, lambda);
      }

      directionalCascadeCount_ = DIRECTIONAL_CASCADE_COUNT;

      for (int cascade = 0; cascade < DIRECTIONAL_CASCADE_COUNT; cascade++)
      {
        float const sliceNear = (cascade == 0) ? nearPlane : splits[cascade - 1];
        float       sliceFar  = splits[cascade];

        // Add cascade overlap: extend each cascade's far plane by 30% into the next cascade
        // This ensures geometry is rendered in both cascades at boundaries for smooth blending
        if (cascade < DIRECTIONAL_CASCADE_COUNT - 1)
        {
          float const nextFar  = splits[cascade + 1];
          float const overlap  = (nextFar - sliceFar) * 0.3f;
          sliceFar            += overlap;
        }

        directionalCascadeSplits_[cascade] = splits[cascade];  // Store original split for shader

        if (shadowLightCount_ >= MAX_SHADOW_MAPS)
        {
          break;
        }

        lightSpaceMatrices_[shadowLightCount_] = calculateDirectionalCascadeMatrix(lightDir, frameInfo.camera, sliceNear, sliceFar, cascade, shadowMapSize_, nullptr, nullptr);

        renderToShadowMapMesh(frameInfo, *shadowMaps_[shadowLightCount_], lightSpaceMatrices_[shadowLightCount_]);
        shadowLightCount_++;
      }

      break; // Only one directional light
    }

    // Render shadow maps for spotlights
    auto spotView = frameInfo.scene->getRegistry().view<SpotLightComponent, TransformComponent>();
    for (auto entity : spotView)
    {
      if (shadowLightCount_ >= MAX_SHADOW_MAPS)
      {
        break;
      }
      auto [spotLight, transform] = spotView.get<SpotLightComponent, TransformComponent>(entity);

      glm::vec3 const position  = transform.translation;
      glm::vec3 const direction = transform.getForwardDir();

      float const outerCutoffDegrees = spotLight.outerCutoffAngle;
      float const range              = 50.0f;

      lightSpaceMatrices_[shadowLightCount_] = calculateSpotLightMatrix(position, direction, outerCutoffDegrees, range);
      renderToShadowMapMesh(frameInfo, *shadowMaps_[shadowLightCount_], lightSpaceMatrices_[shadowLightCount_]);
      shadowLightCount_++;
    }

    // Render cube shadow maps for point lights
    renderPointLightShadowMaps(frameInfo);
  }

  void ShadowSystem::renderPointLightShadowMaps(FrameInfo& frameInfo)
  {
    cubeShadowLightCount_ = 0;

    auto view = frameInfo.scene->getRegistry().view<PointLightComponent, TransformComponent>();
    for (auto entity : view)
    {
      if (cubeShadowLightCount_ >= MAX_CUBE_SHADOW_MAPS)
      {
        break;
      }
      auto [pointLight, transform] = view.get<PointLightComponent, TransformComponent>(entity);

      glm::vec3 const position = transform.translation;
      float const     range    = 25.0f;

      pointLightPositions_[cubeShadowLightCount_] = position;
      pointLightRanges_[cubeShadowLightCount_]    = range;

      renderToCubeShadowMap(frameInfo, *cubeShadowMaps_[cubeShadowLightCount_], position, range);

      cubeShadowLightCount_++;
    }
  }

  glm::mat4 ShadowSystem::calculatePointLightMatrix(const glm::vec3& position, int face, float range)
  {
    float const nearPlane  = 0.1f;
    glm::mat4   projection = glm::perspective(glm::radians(90.0f), 1.0f, nearPlane, range);
    projection[1][1] *= -1;

    glm::mat4 const view = CubeShadowMap::getFaceViewMatrix(position, face);

    return projection * view;
  }

  void ShadowSystem::renderToCubeShadowMap(FrameInfo& frameInfo, CubeShadowMap& cubeShadowMap, const glm::vec3& position, float range)
  {
    for (int face = 0; face < 6; face++)
    {
      glm::mat4 const lightSpaceMatrix = calculatePointLightMatrix(position, face, range);
      renderToCubeFaceMesh(frameInfo, cubeShadowMap, face, lightSpaceMatrix, position, range);
    }
  }

  void ShadowSystem::renderToCubeFaceMesh(FrameInfo& frameInfo, CubeShadowMap& cubeShadowMap, int face, const glm::mat4& lightSpaceMatrix, const glm::vec3& lightPos, float farPlane)
  {
    cubeShadowMap.beginRenderPass(frameInfo.commandBuffer, face);
    cubeMeshPipeline_->bind(frameInfo.commandBuffer);

    // Render all shadow-casting objects using mesh shaders
    auto view = frameInfo.scene->getRegistry().view<ModelComponent, TransformComponent>();
    for (auto entity : view)
    {
      auto [modelComp, transform] = view.get<ModelComponent, TransformComponent>(entity);
      if (!modelComp.model || modelComp.model->getMeshletCount() == 0)
      {
        continue;
      }

      const auto& model = modelComp.model;

      for (const auto& subMesh : model->getSubMeshes())
      {
        if (subMesh.meshletCount == 0)
        {
          continue;
        }

        CubeShadowMeshPushConstants push{};
        push.modelMatrix             = transform.modelTransform();
        push.lightSpaceMatrix        = lightSpaceMatrix;
        push.lightPosAndFarPlane     = glm::vec4(lightPos, farPlane);
        push.meshletBufferAddress    = model->getMeshletBufferAddress();
        push.meshletVerticesAddress  = model->getMeshletVerticesAddress();
        push.meshletTrianglesAddress = model->getMeshletTrianglesAddress();
        push.vertexBufferAddress     = model->getVertexBufferAddress();
        push.meshletOffset           = subMesh.meshletOffset;
        push.meshletCount            = subMesh.meshletCount;

        vkCmdPushConstants(frameInfo.commandBuffer, cubeMeshPipelineLayout_, VK_SHADER_STAGE_TASK_BIT_EXT | VK_SHADER_STAGE_MESH_BIT_EXT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(push), &push);

        uint32_t const groupCount = (subMesh.meshletCount + 31) / 32;
        device_.vkCmdDrawMeshTasksEXT(frameInfo.commandBuffer, groupCount, 1, 1);
      }
    }

    engine::CubeShadowMap::endRenderPass(frameInfo.commandBuffer);
  }

} // namespace engine
