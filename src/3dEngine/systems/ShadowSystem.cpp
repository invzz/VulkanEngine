#include "3dEngine/systems/ShadowSystem.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <iostream>

#include "3dEngine/GameObject.hpp"
#include "3dEngine/GameObjectManager.hpp"
#include "3dEngine/Model.hpp"
#include "3dEngine/ansi_colors.hpp"

namespace engine {

  struct ShadowPushConstants
  {
    glm::mat4 modelMatrix;
    glm::mat4 lightSpaceMatrix;
  };

  ShadowSystem::ShadowSystem(Device& device, uint32_t shadowMapSize) : device_{device}
  {
    // Create multiple shadow maps
    for (int i = 0; i < MAX_SHADOW_MAPS; i++)
    {
      shadowMaps_.push_back(std::make_unique<ShadowMap>(device, shadowMapSize, shadowMapSize));
      lightSpaceMatrices_[i] = glm::mat4(1.0f);
    }

    createPipelineLayout();
    createPipeline();

    std::cout << "[" << GREEN << "ShadowSystem" << RESET << "] Initialized with " << MAX_SHADOW_MAPS << " shadow maps (" << shadowMapSize << "x"
              << shadowMapSize << ")" << std::endl;
  }

  ShadowSystem::~ShadowSystem()
  {
    if (pipelineLayout_ != VK_NULL_HANDLE)
    {
      vkDestroyPipelineLayout(device_.device(), pipelineLayout_, nullptr);
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
    if (frameInfo.objectManager)
    {
      for (auto& [id, obj] : frameInfo.objectManager->getAllObjects())
      {
        if (!obj.model) continue;

        ShadowPushConstants push{};
        push.modelMatrix      = obj.transform.modelTransform();
        push.lightSpaceMatrix = lightSpaceMatrix;

        vkCmdPushConstants(frameInfo.commandBuffer, pipelineLayout_, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(ShadowPushConstants), &push);

        obj.model->bind(frameInfo.commandBuffer);
        obj.model->draw(frameInfo.commandBuffer);
      }
    }

    // End shadow render pass
    shadowMap.endRenderPass(frameInfo.commandBuffer);
  }

  void ShadowSystem::renderShadowMaps(FrameInfo& frameInfo, float sceneRadius)
  {
    shadowLightCount_     = 0;
    glm::vec3 sceneCenter = glm::vec3(0.0f);

    if (!frameInfo.objectManager) return;

    // Render shadow map for first directional light
    auto& dirLights = frameInfo.objectManager->getDirectionalLights();
    if (!dirLights.empty() && shadowLightCount_ < MAX_SHADOW_MAPS)
    {
      glm::vec3 lightDir                     = dirLights[0]->transform.getForwardDir();
      lightSpaceMatrices_[shadowLightCount_] = calculateDirectionalLightMatrix(lightDir, sceneCenter, sceneRadius);
      renderToShadowMap(frameInfo, *shadowMaps_[shadowLightCount_], lightSpaceMatrices_[shadowLightCount_]);
      shadowLightCount_++;
    }

    // Render shadow maps for spotlights
    auto& spotLights = frameInfo.objectManager->getSpotLights();
    for (size_t i = 0; i < spotLights.size() && shadowLightCount_ < MAX_SHADOW_MAPS; i++)
    {
      auto&     spot      = spotLights[i];
      glm::vec3 position  = spot->transform.translation;
      glm::vec3 direction = spot->transform.getForwardDir();

      // Get outer cutoff from light component (or use default 45 degrees)
      float outerCutoffDegrees = 45.0f;
      if (spot->spotLight)
      {
        outerCutoffDegrees = spot->spotLight->outerCutoffAngle;
      }

      // Estimate range from attenuation
      float range = 50.0f;

      lightSpaceMatrices_[shadowLightCount_] = calculateSpotLightMatrix(position, direction, outerCutoffDegrees, range);
      renderToShadowMap(frameInfo, *shadowMaps_[shadowLightCount_], lightSpaceMatrices_[shadowLightCount_]);
      shadowLightCount_++;
    }
  }

} // namespace engine
