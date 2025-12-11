#include "Engine/Systems/LightSystem.hpp"

#include "Engine/Core/Exceptions.hpp"
#include "Engine/Scene/components/DirectionalLightComponent.hpp"
#include "Engine/Scene/components/PointLightComponent.hpp"
#include "Engine/Scene/components/SpotLightComponent.hpp"
#include "Engine/Scene/components/TransformComponent.hpp"

// Ensure GLM uses radians for all angle measurements
#define GLM_FORCE_RADIANS
// Ensure depth range is [0, 1] for Vulkan
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <GLFW/glfw3.h>

#include <array>
#include <cmath>
#include <glm/common.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <iostream>
#include <stdexcept>

#include "Engine/Systems/LightSystem.hpp"

namespace engine {
  struct PointLightPushConstants
  {
    /* data */
    glm::vec4 position{}; // w component unused
    glm::vec4 color{};    // w component is intensity
    float     radius{};
  };

  LightSystem::LightSystem(Device& device, VkRenderPass renderPass, VkDescriptorSetLayout globalSetLayout) : device(device)
  {
    createPipelineLayout(globalSetLayout);
    createPipeline(renderPass);
    createDirectionalLightPipelineLayout(globalSetLayout);
    createDirectionalLightPipeline(renderPass);
    createSpotLightPipelineLayout(globalSetLayout);
    createSpotLightPipeline(renderPass);
  }

  void LightSystem::createPipelineLayout(VkDescriptorSetLayout globalSetLayout)
  {
    VkPushConstantRange pushConstantRange{
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            .offset     = 0,
            .size       = sizeof(PointLightPushConstants),
    };

    std::vector<VkDescriptorSetLayout> descriptorSetLayouts{globalSetLayout};

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
  LightSystem::~LightSystem()
  {
    vkDestroyPipelineLayout(device.device(), pipelineLayout, nullptr);
    vkDestroyPipelineLayout(device.device(), directionalPipelineLayout, nullptr);
    vkDestroyPipelineLayout(device.device(), spotPipelineLayout, nullptr);
  }

  void LightSystem::createPipeline(VkRenderPass renderPass)
  {
    assert(pipelineLayout != VK_NULL_HANDLE && "Pipeline layout must be created before pipeline.");

    PipelineConfigInfo pipelineConfig{};
    Pipeline::defaultPipelineConfigInfo(pipelineConfig);
    pipelineConfig.attributeDescriptions.clear();
    pipelineConfig.bindingDescriptions.clear();
    pipelineConfig.renderPass     = renderPass;
    pipelineConfig.pipelineLayout = pipelineLayout;
    pipeline = std::make_unique<Pipeline>(device, SHADER_PATH "/point_light.vert.spv", SHADER_PATH "/point_light.frag.spv", pipelineConfig);
  }

  void LightSystem::render(FrameInfo& frameInfo)
  {
    pipeline->bind(frameInfo.commandBuffer);

    vkCmdBindDescriptorSets(frameInfo.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &frameInfo.globalDescriptorSet, 0, nullptr);

    auto view = frameInfo.scene->getRegistry().view<PointLightComponent, TransformComponent>();
    for (auto entity : view)
    {
      auto [pointLight, transform] = view.get<PointLightComponent, TransformComponent>(entity);

      PointLightPushConstants push{};
      push.position = glm::vec4(transform.translation, 1.f);
      push.color    = glm::vec4(pointLight.color, pointLight.intensity);
      push.radius   = transform.scale.x;

      vkCmdPushConstants(frameInfo.commandBuffer,
                         pipelineLayout,
                         VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                         0,
                         sizeof(PointLightPushConstants),
                         &push);
      // inefficient to draw a quad for each light, but okay for demo purposes
      vkCmdDraw(frameInfo.commandBuffer, 6, 1, 0, 0);
    }

    // Render directional lights as arrows
    directionalPipeline->bind(frameInfo.commandBuffer);
    vkCmdBindDescriptorSets(frameInfo.commandBuffer,
                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                            directionalPipelineLayout,
                            0,
                            1,
                            &frameInfo.globalDescriptorSet,
                            0,
                            nullptr);

    auto dirView = frameInfo.scene->getRegistry().view<DirectionalLightComponent, TransformComponent>();
    for (auto entity : dirView)
    {
      auto [dirLight, transform] = dirView.get<DirectionalLightComponent, TransformComponent>(entity);

      // Create a model matrix that orients the arrow in the light direction
      glm::mat4 modelMatrix = glm::mat4(1.0f);
      modelMatrix           = glm::translate(modelMatrix, transform.translation);

      // Apply rotation to orient arrow
      modelMatrix = glm::rotate(modelMatrix, transform.rotation.y, glm::vec3(0.0f, 1.0f, 0.0f));
      modelMatrix = glm::rotate(modelMatrix, transform.rotation.x, glm::vec3(1.0f, 0.0f, 0.0f));
      modelMatrix = glm::rotate(modelMatrix, transform.rotation.z, glm::vec3(0.0f, 0.0f, 1.0f));

      struct DirectionalLightPush
      {
        glm::mat4 modelMatrix;
        glm::vec4 color;
      } push;

      push.modelMatrix = modelMatrix;
      push.color       = glm::vec4(dirLight.color, dirLight.intensity);

      vkCmdPushConstants(frameInfo.commandBuffer, directionalPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(push), &push);

      vkCmdDraw(frameInfo.commandBuffer, 18, 1, 0, 0); // 18 vertices for arrow
    }

    // Render spot lights as cones
    spotPipeline->bind(frameInfo.commandBuffer);
    vkCmdBindDescriptorSets(frameInfo.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, spotPipelineLayout, 0, 1, &frameInfo.globalDescriptorSet, 0, nullptr);

    auto spotView = frameInfo.scene->getRegistry().view<SpotLightComponent, TransformComponent>();
    for (auto entity : spotView)
    {
      auto [spotLight, transform] = spotView.get<SpotLightComponent, TransformComponent>(entity);

      // Create a model matrix that positions and orients the cone
      glm::mat4 modelMatrix = glm::mat4(1.0f);
      modelMatrix           = glm::translate(modelMatrix, transform.translation);

      // Apply rotation to orient cone
      modelMatrix = glm::rotate(modelMatrix, transform.rotation.y, glm::vec3(0.0f, 1.0f, 0.0f));
      modelMatrix = glm::rotate(modelMatrix, transform.rotation.x, glm::vec3(1.0f, 0.0f, 0.0f));
      modelMatrix = glm::rotate(modelMatrix, transform.rotation.z, glm::vec3(0.0f, 0.0f, 1.0f));

      struct SpotLightPush
      {
        glm::mat4 modelMatrix;
        glm::vec4 color;
        float     coneAngle;
      } push;

      push.modelMatrix = modelMatrix;
      push.color       = glm::vec4(spotLight.color, spotLight.intensity);
      push.coneAngle   = glm::radians(spotLight.outerCutoffAngle);

      vkCmdPushConstants(frameInfo.commandBuffer, spotPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(push), &push);

      // Draw cone: 32 segments * 3 vertices per triangle = 96 vertices
      vkCmdDraw(frameInfo.commandBuffer, 96, 1, 0, 0);
    }
  }

  void LightSystem::updateTargetLockedLight(entt::entity entity, Scene* scene)
  {
    auto& registry = scene->getRegistry();

    // Update directional light target tracking
    if (registry.all_of<DirectionalLightComponent>(entity))
    {
      auto& dirLight = registry.get<DirectionalLightComponent>(entity);
      if (dirLight.useTargetPoint)
      {
        registry.get<TransformComponent>(entity).lookAt(dirLight.targetPoint);
      }
    }

    // Update spot light target tracking
    if (registry.all_of<SpotLightComponent>(entity))
    {
      auto& spotLight = registry.get<SpotLightComponent>(entity);
      if (spotLight.useTargetPoint)
      {
        registry.get<TransformComponent>(entity).lookAt(spotLight.targetPoint);
      }
    }
  }

  void LightSystem::update(FrameInfo& frameInfo, GlobalUbo& ubo) const
  {
    ubo.pointLightCount       = 0;
    ubo.directionalLightCount = 0;
    ubo.spotLightCount        = 0;

    auto rotateLight = glm::rotate(glm::mat4(1.f), frameInfo.frameTime, glm::vec3(0.f, -1.f, 0.f));

    auto& registry = frameInfo.scene->getRegistry();

    // Process point lights
    auto pointView = registry.view<TransformComponent, PointLightComponent>();
    for (auto entity : pointView)
    {
      auto [transform, pointLight] = pointView.get<TransformComponent, PointLightComponent>(entity);

      assert(ubo.pointLightCount < maxLightCount && "Exceeded maximum point light count!");
      ubo.pointLights[ubo.pointLightCount].position = glm::vec4(transform.translation, 1.f);
      ubo.pointLights[ubo.pointLightCount].color    = glm::vec4(pointLight.color, pointLight.intensity);
      ubo.pointLightCount++;
    }

    // Process directional lights
    auto dirView = registry.view<TransformComponent, DirectionalLightComponent>();
    for (auto entity : dirView)
    {
      auto [transform, dirLight] = dirView.get<TransformComponent, DirectionalLightComponent>(entity);

      assert(ubo.directionalLightCount < maxLightCount && "Exceeded maximum directional light count!");

      // Update rotation to look at target if enabled
      if (dirLight.useTargetPoint)
      {
        transform.lookAt(dirLight.targetPoint);
      }

      glm::vec3 direction                                        = transform.getForwardDir();
      ubo.directionalLights[ubo.directionalLightCount].direction = glm::vec4(glm::normalize(direction), 0.f);
      ubo.directionalLights[ubo.directionalLightCount].color     = glm::vec4(dirLight.color, dirLight.intensity);
      ubo.directionalLightCount++;
    }

    // Process spot lights
    auto spotView = registry.view<TransformComponent, SpotLightComponent>();
    for (auto entity : spotView)
    {
      auto [transform, spotLight] = spotView.get<TransformComponent, SpotLightComponent>(entity);

      assert(ubo.spotLightCount < maxLightCount && "Exceeded maximum spot light count!");

      // Update rotation to look at target if enabled
      if (spotLight.useTargetPoint)
      {
        transform.lookAt(spotLight.targetPoint);
      }

      glm::vec3 direction = transform.getForwardDir();

      ubo.spotLights[ubo.spotLightCount].position       = glm::vec4(transform.translation, 1.f);
      ubo.spotLights[ubo.spotLightCount].direction      = glm::vec4(glm::normalize(direction), glm::cos(glm::radians(spotLight.innerCutoffAngle)));
      ubo.spotLights[ubo.spotLightCount].color          = glm::vec4(spotLight.color, spotLight.intensity);
      ubo.spotLights[ubo.spotLightCount].outerCutoff    = glm::cos(glm::radians(spotLight.outerCutoffAngle));
      ubo.spotLights[ubo.spotLightCount].constantAtten  = spotLight.constantAttenuation;
      ubo.spotLights[ubo.spotLightCount].linearAtten    = spotLight.linearAttenuation;
      ubo.spotLights[ubo.spotLightCount].quadraticAtten = spotLight.quadraticAttenuation;
      ubo.spotLightCount++;
    }
  }

  void LightSystem::createDirectionalLightPipelineLayout(VkDescriptorSetLayout globalSetLayout)
  {
    VkPushConstantRange pushConstantRange{
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            .offset     = 0,
            .size       = sizeof(glm::mat4) + sizeof(glm::vec4), // modelMatrix + color
    };

    std::vector<VkDescriptorSetLayout> descriptorSetLayouts{globalSetLayout};

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{
            .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount         = static_cast<uint32_t>(descriptorSetLayouts.size()),
            .pSetLayouts            = descriptorSetLayouts.data(),
            .pushConstantRangeCount = 1,
            .pPushConstantRanges    = &pushConstantRange,
    };

    if (vkCreatePipelineLayout(device.device(), &pipelineLayoutInfo, nullptr, &directionalPipelineLayout) != VK_SUCCESS)
    {
      throw engine::RuntimeException("failed to create directional light pipeline layout!");
    }
  }

  void LightSystem::createDirectionalLightPipeline(VkRenderPass renderPass)
  {
    assert(directionalPipelineLayout != VK_NULL_HANDLE && "Pipeline layout must be created before pipeline.");

    PipelineConfigInfo pipelineConfig{};
    Pipeline::defaultPipelineConfigInfo(pipelineConfig);
    pipelineConfig.attributeDescriptions.clear();
    pipelineConfig.bindingDescriptions.clear();
    pipelineConfig.renderPass                 = renderPass;
    pipelineConfig.pipelineLayout             = directionalPipelineLayout;
    pipelineConfig.inputAssemblyInfo.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
    directionalPipeline =
            std::make_unique<Pipeline>(device, SHADER_PATH "/directional_light.vert.spv", SHADER_PATH "/directional_light.frag.spv", pipelineConfig);
  }

  void LightSystem::createSpotLightPipelineLayout(VkDescriptorSetLayout globalSetLayout)
  {
    VkPushConstantRange pushConstantRange{
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            .offset     = 0,
            .size       = sizeof(glm::mat4) + sizeof(glm::vec4) + sizeof(float), // modelMatrix + color + coneAngle
    };

    std::vector<VkDescriptorSetLayout> descriptorSetLayouts{globalSetLayout};

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{
            .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount         = static_cast<uint32_t>(descriptorSetLayouts.size()),
            .pSetLayouts            = descriptorSetLayouts.data(),
            .pushConstantRangeCount = 1,
            .pPushConstantRanges    = &pushConstantRange,
    };

    if (vkCreatePipelineLayout(device.device(), &pipelineLayoutInfo, nullptr, &spotPipelineLayout) != VK_SUCCESS)
    {
      throw engine::RuntimeException("failed to create spot light pipeline layout!");
    }
  }

  void LightSystem::createSpotLightPipeline(VkRenderPass renderPass)
  {
    assert(spotPipelineLayout != VK_NULL_HANDLE && "Pipeline layout must be created before pipeline.");

    PipelineConfigInfo pipelineConfig{};
    Pipeline::defaultPipelineConfigInfo(pipelineConfig);
    pipelineConfig.attributeDescriptions.clear();
    pipelineConfig.bindingDescriptions.clear();
    pipelineConfig.renderPass                        = renderPass;
    pipelineConfig.pipelineLayout                    = spotPipelineLayout;
    pipelineConfig.inputAssemblyInfo.topology        = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    pipelineConfig.rasterizationInfo.cullMode        = VK_CULL_MODE_NONE;
    pipelineConfig.depthStencilInfo.depthWriteEnable = VK_FALSE;

    // Enable alpha blending for semi-transparent cone
    pipelineConfig.colorBlendAttachment.blendEnable         = VK_TRUE;
    pipelineConfig.colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    pipelineConfig.colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    pipelineConfig.colorBlendAttachment.colorBlendOp        = VK_BLEND_OP_ADD;
    pipelineConfig.colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    pipelineConfig.colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    pipelineConfig.colorBlendAttachment.alphaBlendOp        = VK_BLEND_OP_ADD;

    spotPipeline = std::make_unique<Pipeline>(device, SHADER_PATH "/spot_light.vert.spv", SHADER_PATH "/spot_light.frag.spv", pipelineConfig);
  }
} // namespace engine
