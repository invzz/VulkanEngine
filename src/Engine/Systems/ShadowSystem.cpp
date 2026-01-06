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
#include "Engine/Resources/Model.hpp"
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

  struct ShadowPushConstants
  {
    glm::mat4 modelMatrix;
    glm::mat4 lightSpaceMatrix;
  };

  struct CubeShadowPushConstants
  {
    glm::mat4 modelMatrix;
    glm::mat4 lightSpaceMatrix;
    glm::vec4 lightPosAndFarPlane; // xyz = light position, w = far plane
  };

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

    createPipelineLayout();
    createPipeline();
    createCubeShadowPipelineLayout();
    createCubeShadowPipeline();

    std::cout << "[" << GREEN << "ShadowSystem" << RESET << "] Initialized with " << MAX_SHADOW_MAPS << " 2D shadow maps and " << MAX_CUBE_SHADOW_MAPS << " cube shadow maps (" << shadowMapSize << "x"
              << shadowMapSize << ")" << '\n';
  }

  ShadowSystem::~ShadowSystem()
  {
    if (pipelineLayout_ != VK_NULL_HANDLE)
    {
      vkDestroyPipelineLayout(device_.device(), pipelineLayout_, nullptr);
    }
    if (cubePipelineLayout_ != VK_NULL_HANDLE)
    {
      vkDestroyPipelineLayout(device_.device(), cubePipelineLayout_, nullptr);
    }
  }

  void ShadowSystem::createPipelineLayout()
  {
    VkPushConstantRange pushConstantRange{};
    // Shadow shaders use vertex + fragment stages for small push constants
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pushConstantRange.offset     = 0;
    pushConstantRange.size       = sizeof(ShadowPushConstants);

    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount         = 0;
    layoutInfo.pSetLayouts            = nullptr;
    layoutInfo.pushConstantRangeCount = 1;
    layoutInfo.pPushConstantRanges    = &pushConstantRange;

    if (vkCreatePipelineLayout(device_.device(), &layoutInfo, nullptr, &pipelineLayout_) != VK_SUCCESS)
    {
      throw std::runtime_error("Failed to create shadow pipeline layout");
    }
  }

  void ShadowSystem::createPipeline()
  {
    PipelineConfigInfo configInfo{};
    Pipeline::defaultPipelineConfigInfo(configInfo);

    // Only need position for shadow mapping
    configInfo.bindingDescriptions   = Model::Vertex::getBindingDescriptions();
    configInfo.attributeDescriptions = Model::Vertex::getAttributeDescriptions();

    // No color attachment - depth only
    configInfo.colorBlendInfo.attachmentCount = 0;
    configInfo.colorBlendAttachment           = {}; // Not used

    // Depth bias to prevent shadow acne
    configInfo.rasterizationInfo.depthBiasEnable         = VK_TRUE;
    configInfo.rasterizationInfo.depthBiasConstantFactor = 1.25f;
    configInfo.rasterizationInfo.depthBiasSlopeFactor    = 1.75f;

    // Cull front faces to reduce peter-panning
    // configInfo.rasterizationInfo.cullMode = VK_CULL_MODE_FRONT_BIT;
    configInfo.rasterizationInfo.cullMode = VK_CULL_MODE_NONE;

    // Use the render pass from the first shadow map (all are identical)
    configInfo.renderPass     = shadowMaps_[0]->getRenderPass();
    configInfo.pipelineLayout = pipelineLayout_;

    pipeline_ = std::make_unique<Pipeline>(device_, std::string(SHADER_PATH) + R"(shadow.vert.spv)", std::string(SHADER_PATH) + R"(shadow.frag.spv)", configInfo);
  }

  glm::mat4 ShadowSystem::calculateDirectionalCascadeMatrix(const glm::vec3& lightDirection, const Camera& camera, float cascadeNear, float cascadeFar) const
  {
    // Fit a directional shadow frustum to a slice of the camera view frustum (CSM).
    // cascadeNear/cascadeFar are view-space distances along the camera forward axis.

    glm::vec3 const lightDir = glm::normalize(lightDirection);

    glm::mat4 const proj    = camera.getProjectionMatrix();
    glm::mat4 const invView = camera.getInverseView();

    // Derive perspective params from Vulkan-style projection matrix.
    // Camera::setPerspectiveProjection encodes:
    //   proj[2][2] = far/(far-near)
    //   proj[3][2] = -(far*near)/(far-near)
    // So near = -B/A.
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

    float const tanHalfFovy = 1.0f / glm::max(proj[1][1], 1e-6f);
    float const aspect      = proj[1][1] / glm::max(proj[0][0], 1e-6f);

    float const sliceNear = glm::clamp(cascadeNear, nearPlane, farPlane - 0.01f);
    float const sliceFar  = glm::clamp(cascadeFar, sliceNear + 0.01f, farPlane);

    float const nearHeight = 2.0f * tanHalfFovy * sliceNear;
    float const nearWidth  = nearHeight * aspect;
    float const farHeight  = 2.0f * tanHalfFovy * sliceFar;
    float const farWidth   = farHeight * aspect;

    glm::vec3 const camPos   = glm::vec3(invView[3]);
    glm::vec3 const camRight = glm::normalize(glm::vec3(invView[0]));
    glm::vec3 const camUp    = glm::normalize(glm::vec3(invView[1]));
    glm::vec3 const camFwd   = glm::normalize(glm::vec3(invView[2]));

    // 8 frustum corners in world space (camera forward is +Z in this engine).
    glm::vec3 frustumCorners[8];

    float const nearZ = sliceNear;
    float const farZ  = sliceFar;
    float const nx    = nearWidth * 0.5f;
    float const ny    = nearHeight * 0.5f;
    float const fx    = farWidth * 0.5f;
    float const fy    = farHeight * 0.5f;

    // Near plane
    frustumCorners[0] = camPos + camFwd * nearZ - camRight * nx - camUp * ny;
    frustumCorners[1] = camPos + camFwd * nearZ + camRight * nx - camUp * ny;
    frustumCorners[2] = camPos + camFwd * nearZ + camRight * nx + camUp * ny;
    frustumCorners[3] = camPos + camFwd * nearZ - camRight * nx + camUp * ny;
    // Far plane
    frustumCorners[4] = camPos + camFwd * farZ - camRight * fx - camUp * fy;
    frustumCorners[5] = camPos + camFwd * farZ + camRight * fx - camUp * fy;
    frustumCorners[6] = camPos + camFwd * farZ + camRight * fx + camUp * fy;
    frustumCorners[7] = camPos + camFwd * farZ - camRight * fx + camUp * fy;

    glm::vec3 frustumCenter{0.0f};
    for (glm::vec3 const& c : frustumCorners)
    {
      frustumCenter += c;
    }
    frustumCenter /= 8.0f;

    // Handle edge case where light is directly above/below.
    glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
    if (glm::abs(glm::dot(lightDir, up)) > 0.99f)
    {
      up = glm::vec3(0.0f, 0.0f, 1.0f);
    }

    // Place the light back along its direction; exact distance doesn't matter as
    // long as we compute extents in this same view space.
    glm::vec3 const lightPos  = frustumCenter - lightDir * (sliceFar * 2.0f);
    glm::mat4 const lightView = glm::lookAt(lightPos, frustumCenter, up);

    glm::vec3 minLS(std::numeric_limits<float>::infinity());
    glm::vec3 maxLS(-std::numeric_limits<float>::infinity());
    for (glm::vec3 const& cornerWS : frustumCorners)
    {
      glm::vec4 const cornerLS4 = lightView * glm::vec4(cornerWS, 1.0f);
      glm::vec3 const cornerLS  = glm::vec3(cornerLS4);
      minLS                     = glm::min(minLS, cornerLS);
      maxLS                     = glm::max(maxLS, cornerLS);
    }

    // Stabilize the projection to avoid shimmering when the camera moves.
    // Snap the ortho bounds to the shadow-map texel grid in light space.
    {
      glm::vec3 const centerLS = 0.5f * (minLS + maxLS);
      float const     extentX  = maxLS.x - minLS.x;
      float const     extentY  = maxLS.y - minLS.y;
      float const     extent   = glm::max(extentX, extentY);

      float const halfExtent = 0.5f * extent;

      float const texelSize     = extent / glm::max(1.0f, static_cast<float>(shadowMapSize_));
      glm::vec3   snappedCenter = centerLS;
      if (texelSize > 0.0f)
      {
        snappedCenter.x = glm::floor(snappedCenter.x / texelSize) * texelSize;
        snappedCenter.y = glm::floor(snappedCenter.y / texelSize) * texelSize;
      }

      minLS.x = snappedCenter.x - halfExtent;
      maxLS.x = snappedCenter.x + halfExtent;
      minLS.y = snappedCenter.y - halfExtent;
      maxLS.y = snappedCenter.y + halfExtent;
    }

    // Expand depth (light-space Z) to include potential casters not on the receiver frustum slice.
    // This is important indoors (e.g. ceilings/roofs): the receiver slice may be mostly floor,
    // but overhead occluders still need to be inside the shadow frustum.
    float const extentX = maxLS.x - minLS.x;
    float const extentY = maxLS.y - minLS.y;
    float const extentZ = maxLS.z - minLS.z;
    float const extent  = glm::max(glm::max(extentX, extentY), extentZ);

    // Scale padding with cascade size to keep casters around the receiver area.
    float const depthPadding = glm::max(10.0f, extent);
    minLS.z -= depthPadding;
    maxLS.z += depthPadding;

    // glm::lookAt uses a right-handed view where forward is -Z.
    // Visible points tend to have negative Z in light view space, while near/far
    // passed to ortho are positive distances.
    float const orthoNear = glm::max(0.01f, -maxLS.z);
    float const orthoFar  = glm::max(orthoNear + 0.01f, -minLS.z);

    glm::mat4 lightProj = glm::orthoZO(minLS.x, maxLS.x, minLS.y, maxLS.y, orthoNear, orthoFar);
    // Vulkan clip space correction (Y flip)
    lightProj[1][1] *= -1;

    return lightProj * lightView;
  }

  glm::mat4 ShadowSystem::calculateSpotLightMatrix(const glm::vec3& position, const glm::vec3& direction, float outerCutoffDegrees, float range)
  {
    glm::vec3 const lightDir = glm::normalize(direction);

    // Handle edge case where light points directly up/down
    glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
    if (glm::abs(glm::dot(lightDir, up)) > 0.99f)
    {
      up = glm::vec3(0.0f, 0.0f, 1.0f);
    }

    glm::mat4 const lightView = glm::lookAt(position, position + lightDir, up);

    // Perspective projection based on spotlight cone angle
    // outerCutoffDegrees is the outer cone angle in degrees
    // FOV should be 2 * angle to cover the full cone, add some margin
    float const fov       = glm::radians((outerCutoffDegrees * 2.0f) + 5.0f); // Add 5 degree margin
    float const nearPlane = 0.1f;
    float const farPlane  = range > 0.0f ? range : 100.0f;

    glm::mat4 lightProj = glm::perspective(fov, 1.0f, nearPlane, farPlane);

    // Vulkan clip space correction (Y flip)
    lightProj[1][1] *= -1;

    return lightProj * lightView;
  }

  void ShadowSystem::renderToShadowMap(FrameInfo& frameInfo, ShadowMap& shadowMap, const glm::mat4& lightSpaceMatrix)
  {
    // Begin shadow render pass

    shadowMap.beginRenderPass(frameInfo.commandBuffer);

    // Bind shadow pipeline
    pipeline_->bind(frameInfo.commandBuffer);

    // Render all objects to shadow map
    auto view = frameInfo.scene->getRegistry().view<ModelComponent, TransformComponent>();
    for (auto entity : view)
    {
      auto [modelComp, transform] = view.get<ModelComponent, TransformComponent>(entity);
      if (!modelComp.model)
      {
        continue;
      }

      ShadowPushConstants push{};
      push.modelMatrix      = transform.modelTransform();
      push.lightSpaceMatrix = lightSpaceMatrix;

      vkCmdPushConstants(frameInfo.commandBuffer, pipelineLayout_, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(push), &push);

      modelComp.model->bind(frameInfo.commandBuffer);
      modelComp.model->draw(frameInfo.commandBuffer);
    }

    // End shadow render pass
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

      // Compute camera near/far from projection (Vulkan-style, depth 0..1).
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

      // Practical split scheme: blend log/linear splits.
      float const lambda = 0.5f;
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
        float const sliceFar  = splits[cascade];

        directionalCascadeSplits_[cascade] = sliceFar;

        if (shadowLightCount_ >= MAX_SHADOW_MAPS)
        {
          break;
        }

        lightSpaceMatrices_[shadowLightCount_] = calculateDirectionalCascadeMatrix(lightDir, frameInfo.camera, sliceNear, sliceFar);
        renderToShadowMap(frameInfo, *shadowMaps_[shadowLightCount_], lightSpaceMatrices_[shadowLightCount_]);
        shadowLightCount_++;
      }

      // Only one directional light shadow for now? The old code took
      // dirLights[0]. I'll break after one.
      break;
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
      renderToShadowMap(frameInfo, *shadowMaps_[shadowLightCount_], lightSpaceMatrices_[shadowLightCount_]);
      shadowLightCount_++;
    }

    // Render cube shadow maps for point lights
    renderPointLightShadowMaps(frameInfo);
  }

  void ShadowSystem::createCubeShadowPipelineLayout()
  {
    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pushConstantRange.offset     = 0;
    pushConstantRange.size       = sizeof(CubeShadowPushConstants);

    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount         = 0;
    layoutInfo.pSetLayouts            = nullptr;
    layoutInfo.pushConstantRangeCount = 1;
    layoutInfo.pPushConstantRanges    = &pushConstantRange;

    if (vkCreatePipelineLayout(device_.device(), &layoutInfo, nullptr, &cubePipelineLayout_) != VK_SUCCESS)
    {
      throw std::runtime_error("Failed to create cube shadow pipeline layout");
    }
  }

  void ShadowSystem::createCubeShadowPipeline()
  {
    PipelineConfigInfo configInfo{};
    Pipeline::defaultPipelineConfigInfo(configInfo);

    // Only need position for shadow mapping
    configInfo.bindingDescriptions   = Model::Vertex::getBindingDescriptions();
    configInfo.attributeDescriptions = Model::Vertex::getAttributeDescriptions();

    // No color attachment - depth only
    configInfo.colorBlendInfo.attachmentCount = 0;
    configInfo.colorBlendAttachment           = {};

    // Depth bias to prevent shadow acne
    configInfo.rasterizationInfo.depthBiasEnable         = VK_TRUE;
    configInfo.rasterizationInfo.depthBiasConstantFactor = 1.25f;
    configInfo.rasterizationInfo.depthBiasSlopeFactor    = 1.75f;

    // No culling for point light shadows to ensure all geometry is captured
    configInfo.rasterizationInfo.cullMode = VK_CULL_MODE_NONE;

    // Use the render pass from the first cube shadow map
    configInfo.renderPass     = cubeShadowMaps_[0]->getRenderPass();
    configInfo.pipelineLayout = cubePipelineLayout_;

    // Use specialized cube shadow shaders that write linear depth
    cubePipeline_ = std::make_unique<Pipeline>(device_, std::string(SHADER_PATH) + R"(cube_shadow.vert.spv)", std::string(SHADER_PATH) + R"(cube_shadow.frag.spv)", configInfo);
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
      float const     range    = 25.0f; // Default range

      // Store light data for UBO
      pointLightPositions_[cubeShadowLightCount_] = position;
      pointLightRanges_[cubeShadowLightCount_]    = range;

      // Render to cube map faces
      renderToCubeShadowMap(frameInfo, *cubeShadowMaps_[cubeShadowLightCount_], position, range);

      cubeShadowLightCount_++;
    }
  }

  glm::mat4 ShadowSystem::calculatePointLightMatrix(const glm::vec3& position, int face, float range)
  {
    float const nearPlane  = 0.1f;
    glm::mat4   projection = glm::perspective(glm::radians(90.0f), 1.0f, nearPlane, range);

    // Vulkan Y flip
    projection[1][1] *= -1;

    glm::mat4 const view = CubeShadowMap::getFaceViewMatrix(position, face);

    return projection * view;
  }

  void ShadowSystem::renderToCubeShadowMap(FrameInfo& frameInfo, CubeShadowMap& cubeShadowMap, const glm::vec3& position, float range)
  {
    for (int face = 0; face < 6; face++)
    {
      glm::mat4 const lightSpaceMatrix = calculatePointLightMatrix(position, face, range);
      renderToCubeFace(frameInfo, cubeShadowMap, face, lightSpaceMatrix, position, range);
    }
  }

  void ShadowSystem::renderToCubeFace(FrameInfo& frameInfo, CubeShadowMap& cubeShadowMap, int face, const glm::mat4& lightSpaceMatrix, const glm::vec3& lightPos, float farPlane)
  {
    // Begin render pass for this face

    cubeShadowMap.beginRenderPass(frameInfo.commandBuffer, face);

    // Bind cube shadow pipeline
    cubePipeline_->bind(frameInfo.commandBuffer);

    // Render all objects
    auto view = frameInfo.scene->getRegistry().view<ModelComponent, TransformComponent>();
    for (auto entity : view)
    {
      auto [modelComp, transform] = view.get<ModelComponent, TransformComponent>(entity);
      if (!modelComp.model)
      {
        continue;
      }

      CubeShadowPushConstants push{};
      push.modelMatrix         = transform.modelTransform();
      push.lightSpaceMatrix    = lightSpaceMatrix;
      push.lightPosAndFarPlane = glm::vec4(lightPos, farPlane);

      vkCmdPushConstants(frameInfo.commandBuffer, cubePipelineLayout_, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(CubeShadowPushConstants), &push);

      modelComp.model->bind(frameInfo.commandBuffer);
      modelComp.model->draw(frameInfo.commandBuffer);
    }

    engine::CubeShadowMap::endRenderPass(frameInfo.commandBuffer);
  }

} // namespace engine
