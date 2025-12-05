#include "Engine/Systems/ShadowSystem.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <iostream>

#include "Engine/Core/ansi_colors.hpp"
#include "Engine/Graphics/CubeShadowMap.hpp"
#include "Engine/Resources/Model.hpp"
#include "Engine/Scene/components/DirectionalLightComponent.hpp"
#include "Engine/Scene/components/ModelComponent.hpp"
#include "Engine/Scene/components/PointLightComponent.hpp"
#include "Engine/Scene/components/SpotLightComponent.hpp"
#include "Engine/Scene/components/TransformComponent.hpp"

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
    for (int i = 0; i < MAX_SHADOW_MAPS; i++)
    {
      shadowMaps_.push_back(std::make_unique<ShadowMap>(device, shadowMapSize, shadowMapSize));
      lightSpaceMatrices_[i] = glm::mat4(1.0f);
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

    std::cout << "[" << GREEN << "ShadowSystem" << RESET << "] Initialized with " << MAX_SHADOW_MAPS << " 2D shadow maps and " << MAX_CUBE_SHADOW_MAPS
              << " cube shadow maps (" << shadowMapSize << "x" << shadowMapSize << ")" << std::endl;
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
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
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
    configInfo.rasterizationInfo.cullMode = VK_CULL_MODE_FRONT_BIT;

    // Use the render pass from the first shadow map (all are identical)
    configInfo.renderPass     = shadowMaps_[0]->getRenderPass();
    configInfo.pipelineLayout = pipelineLayout_;

    pipeline_ = std::make_unique<Pipeline>(device_, SHADER_PATH "/shadow.vert.spv", SHADER_PATH "/shadow.frag.spv", configInfo);
  }

  glm::mat4 ShadowSystem::calculateDirectionalLightMatrix(const glm::vec3& lightDirection, const glm::vec3& sceneCenter, float sceneRadius)
  {
    // lightDirection points FROM light TO scene (the direction light travels)
    glm::vec3 lightDir = glm::normalize(lightDirection);
    glm::vec3 lightPos = sceneCenter - lightDir * sceneRadius * 3.0f;

    // Handle edge case where light is directly above/below
    glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
    if (glm::abs(glm::dot(lightDir, up)) > 0.99f)
    {
      up = glm::vec3(0.0f, 0.0f, 1.0f);
    }
    glm::mat4 lightView = glm::lookAt(lightPos, sceneCenter, up);

    // Orthographic projection that encompasses the scene
    float     orthoSize = sceneRadius * 2.0f;
    float     nearPlane = 0.1f;
    float     farPlane  = sceneRadius * 6.0f;
    glm::mat4 lightProj = glm::ortho(-orthoSize, orthoSize, -orthoSize, orthoSize, nearPlane, farPlane);

    // Vulkan clip space correction (Y flip, Z [0,1])
    lightProj[1][1] *= -1;

    return lightProj * lightView;
  }

  glm::mat4 ShadowSystem::calculateSpotLightMatrix(const glm::vec3& position, const glm::vec3& direction, float outerCutoffDegrees, float range)
  {
    glm::vec3 lightDir = glm::normalize(direction);

    // Handle edge case where light points directly up/down
    glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
    if (glm::abs(glm::dot(lightDir, up)) > 0.99f)
    {
      up = glm::vec3(0.0f, 0.0f, 1.0f);
    }

    glm::mat4 lightView = glm::lookAt(position, position + lightDir, up);

    // Perspective projection based on spotlight cone angle
    // outerCutoffDegrees is the outer cone angle in degrees
    // FOV should be 2 * angle to cover the full cone, add some margin
    float fov       = glm::radians(outerCutoffDegrees * 2.0f + 5.0f); // Add 5 degree margin
    float nearPlane = 0.1f;
    float farPlane  = range > 0.0f ? range : 100.0f;

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
      if (!modelComp.model) continue;

      ShadowPushConstants push{};
      push.modelMatrix      = transform.modelTransform();
      push.lightSpaceMatrix = lightSpaceMatrix;

      vkCmdPushConstants(frameInfo.commandBuffer, pipelineLayout_, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(push), &push);

      modelComp.model->bind(frameInfo.commandBuffer);
      modelComp.model->draw(frameInfo.commandBuffer);
    }

    // End shadow render pass
    shadowMap.endRenderPass(frameInfo.commandBuffer);
  }

  void ShadowSystem::renderShadowMaps(FrameInfo& frameInfo, float sceneRadius)
  {
    shadowLightCount_     = 0;
    glm::vec3 sceneCenter = glm::vec3(0.0f);

    // Render shadow map for first directional light
    auto dirView = frameInfo.scene->getRegistry().view<DirectionalLightComponent, TransformComponent>();
    for (auto entity : dirView)
    {
      if (shadowLightCount_ >= MAX_SHADOW_MAPS) break;
      auto [dirLight, transform] = dirView.get<DirectionalLightComponent, TransformComponent>(entity);

      glm::vec3 lightDir                     = transform.getForwardDir();
      lightSpaceMatrices_[shadowLightCount_] = calculateDirectionalLightMatrix(lightDir, sceneCenter, sceneRadius);
      renderToShadowMap(frameInfo, *shadowMaps_[shadowLightCount_], lightSpaceMatrices_[shadowLightCount_]);
      shadowLightCount_++;

      // Only one directional light shadow for now? The old code took dirLights[0].
      // I'll break after one.
      break;
    }

    // Render shadow maps for spotlights
    auto spotView = frameInfo.scene->getRegistry().view<SpotLightComponent, TransformComponent>();
    for (auto entity : spotView)
    {
      if (shadowLightCount_ >= MAX_SHADOW_MAPS) break;
      auto [spotLight, transform] = spotView.get<SpotLightComponent, TransformComponent>(entity);

      glm::vec3 position  = transform.translation;
      glm::vec3 direction = transform.getForwardDir();

      float outerCutoffDegrees = spotLight.outerCutoffAngle;
      float range              = 50.0f;

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
    cubePipeline_ = std::make_unique<Pipeline>(device_, SHADER_PATH "/cube_shadow.vert.spv", SHADER_PATH "/cube_shadow.frag.spv", configInfo);
  }

  void ShadowSystem::renderPointLightShadowMaps(FrameInfo& frameInfo)
  {
    cubeShadowLightCount_ = 0;

    auto view = frameInfo.scene->getRegistry().view<PointLightComponent, TransformComponent>();
    for (auto entity : view)
    {
      if (cubeShadowLightCount_ >= MAX_CUBE_SHADOW_MAPS) break;
      auto [pointLight, transform] = view.get<PointLightComponent, TransformComponent>(entity);

      glm::vec3 position = transform.translation;
      float     range    = 25.0f; // Default range

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
    float     nearPlane  = 0.1f;
    glm::mat4 projection = glm::perspective(glm::radians(90.0f), 1.0f, nearPlane, range);

    // Vulkan Y flip
    projection[1][1] *= -1;

    glm::mat4 view = CubeShadowMap::getFaceViewMatrix(position, face);

    return projection * view;
  }

  void ShadowSystem::renderToCubeShadowMap(FrameInfo& frameInfo, CubeShadowMap& cubeShadowMap, const glm::vec3& position, float range)
  {
    for (int face = 0; face < 6; face++)
    {
      glm::mat4 lightSpaceMatrix = calculatePointLightMatrix(position, face, range);
      renderToCubeFace(frameInfo, cubeShadowMap, face, lightSpaceMatrix, position, range);
    }
  }

  void ShadowSystem::renderToCubeFace(FrameInfo&       frameInfo,
                                      CubeShadowMap&   cubeShadowMap,
                                      int              face,
                                      const glm::mat4& lightSpaceMatrix,
                                      const glm::vec3& lightPos,
                                      float            farPlane)
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
      if (!modelComp.model) continue;

      CubeShadowPushConstants push{};
      push.modelMatrix         = transform.modelTransform();
      push.lightSpaceMatrix    = lightSpaceMatrix;
      push.lightPosAndFarPlane = glm::vec4(lightPos, farPlane);

      vkCmdPushConstants(frameInfo.commandBuffer,
                         cubePipelineLayout_,
                         VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                         0,
                         sizeof(CubeShadowPushConstants),
                         &push);

      modelComp.model->bind(frameInfo.commandBuffer);
      modelComp.model->draw(frameInfo.commandBuffer);
    }

    cubeShadowMap.endRenderPass(frameInfo.commandBuffer);
  }

} // namespace engine
