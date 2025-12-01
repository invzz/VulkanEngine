#include "3dEngine/systems/LightSystem.hpp"

#include "3dEngine/Exceptions.hpp"
#include "3dEngine/GameObjectManager.hpp"

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
#include <stdexcept>

#include "3dEngine/systems/LightSystem.hpp"

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

    // somewhat inefficient to loop over all game objects and check for point light components
    for (const auto& [id, obj] : frameInfo.objectManager->getAllObjects())
    {
      if (!obj.pointLight) continue;

      PointLightPushConstants push{};
      push.position = glm::vec4(obj.transform.translation, 1.f);
      push.color    = glm::vec4(obj.color, obj.pointLight->intensity);
      push.radius   = obj.transform.scale.x;

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

    for (const auto& [id, obj] : frameInfo.objectManager->getAllObjects())
    {
      if (!obj.directionalLight) continue;

      // Create a model matrix that orients the arrow in the light direction
      glm::mat4 modelMatrix = glm::mat4(1.0f);
      modelMatrix           = glm::translate(modelMatrix, obj.transform.translation);

      // Apply rotation to orient arrow
      modelMatrix = glm::rotate(modelMatrix, obj.transform.rotation.y, glm::vec3(0.0f, 1.0f, 0.0f));
      modelMatrix = glm::rotate(modelMatrix, obj.transform.rotation.x, glm::vec3(1.0f, 0.0f, 0.0f));
      modelMatrix = glm::rotate(modelMatrix, obj.transform.rotation.z, glm::vec3(0.0f, 0.0f, 1.0f));

      struct DirectionalLightPush
      {
        glm::mat4 modelMatrix;
        glm::vec4 color;
      } push;

      push.modelMatrix = modelMatrix;
      push.color       = glm::vec4(obj.color, obj.directionalLight->intensity);

      vkCmdPushConstants(frameInfo.commandBuffer, directionalPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(push), &push);

      vkCmdDraw(frameInfo.commandBuffer, 18, 1, 0, 0); // 18 vertices for arrow
    }

    // Render spot lights as cones
    spotPipeline->bind(frameInfo.commandBuffer);
    vkCmdBindDescriptorSets(frameInfo.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, spotPipelineLayout, 0, 1, &frameInfo.globalDescriptorSet, 0, nullptr);

    for (const auto& [id, obj] : frameInfo.objectManager->getAllObjects())
    {
      if (!obj.spotLight) continue;

      // Create a model matrix that positions and orients the cone
      glm::mat4 modelMatrix = glm::mat4(1.0f);
      modelMatrix           = glm::translate(modelMatrix, obj.transform.translation);

      // Apply rotation to orient cone
      modelMatrix = glm::rotate(modelMatrix, obj.transform.rotation.y, glm::vec3(0.0f, 1.0f, 0.0f));
      modelMatrix = glm::rotate(modelMatrix, obj.transform.rotation.x, glm::vec3(1.0f, 0.0f, 0.0f));
      modelMatrix = glm::rotate(modelMatrix, obj.transform.rotation.z, glm::vec3(0.0f, 0.0f, 1.0f));

      struct SpotLightPush
      {
        glm::mat4 modelMatrix;
        glm::vec4 color;
        float     coneAngle;
      } push;

      push.modelMatrix = modelMatrix;
      push.color       = glm::vec4(obj.color, obj.spotLight->intensity);
      push.coneAngle   = glm::radians(obj.spotLight->outerCutoffAngle);

      vkCmdPushConstants(frameInfo.commandBuffer, spotPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(push), &push);

      // Draw cone base circle (16 vertices) + 8 lines from apex = 24 vertices
      vkCmdDraw(frameInfo.commandBuffer, 25, 1, 0, 0);
    }
  }

  void LightSystem::updateTargetLockedLight(GameObject& obj)
  {
    // Update directional light target tracking
    if (obj.directionalLight && obj.directionalLight->useTargetPoint)
    {
      obj.transform.lookAt(obj.directionalLight->targetPoint);
    }

    // Update spot light target tracking
    if (obj.spotLight && obj.spotLight->useTargetPoint)
    {
      obj.transform.lookAt(obj.spotLight->targetPoint);
    }
  }

  void LightSystem::update(FrameInfo& frameInfo, GlobalUbo& ubo) const
  {
    ubo.pointLightCount       = 0;
    ubo.directionalLightCount = 0;
    ubo.spotLightCount        = 0;

    auto rotateLight = glm::rotate(glm::mat4(1.f), frameInfo.frameTime, glm::vec3(0.f, -1.f, 0.f));

    // Use categorized vectors if manager is available
    if (frameInfo.objectManager)
    {
      // Process point lights
      for (auto* obj : frameInfo.objectManager->getPointLights())
      {
        assert(ubo.pointLightCount < maxLightCount && "Exceeded maximum point light count!");
        obj->transform.translation                    = glm::vec3(rotateLight * glm::vec4{obj->transform.translation, 1.f});
        ubo.pointLights[ubo.pointLightCount].position = glm::vec4(obj->transform.translation, 1.f);
        ubo.pointLights[ubo.pointLightCount].color    = glm::vec4(obj->color, obj->pointLight->intensity);
        ubo.pointLightCount++;
      }

      // Process directional lights
      for (auto* obj : frameInfo.objectManager->getDirectionalLights())
      {
        assert(ubo.directionalLightCount < maxLightCount && "Exceeded maximum directional light count!");

        // Update rotation to look at target if enabled
        if (obj->directionalLight->useTargetPoint)
        {
          obj->transform.lookAt(obj->directionalLight->targetPoint);
        }

        glm::vec3 direction                                        = obj->transform.getForwardDir();
        ubo.directionalLights[ubo.directionalLightCount].direction = glm::vec4(glm::normalize(direction), 0.f);
        ubo.directionalLights[ubo.directionalLightCount].color     = glm::vec4(obj->color, obj->directionalLight->intensity);
        ubo.directionalLightCount++;
      }

      // Process spot lights
      for (auto* obj : frameInfo.objectManager->getSpotLights())
      {
        assert(ubo.spotLightCount < maxLightCount && "Exceeded maximum spot light count!");

        // Update rotation to look at target if enabled
        if (obj->spotLight->useTargetPoint)
        {
          obj->transform.lookAt(obj->spotLight->targetPoint);
        }

        glm::vec3 direction = obj->transform.getForwardDir();

        ubo.spotLights[ubo.spotLightCount].position       = glm::vec4(obj->transform.translation, 1.f);
        ubo.spotLights[ubo.spotLightCount].direction      = glm::vec4(glm::normalize(direction), glm::cos(glm::radians(obj->spotLight->innerCutoffAngle)));
        ubo.spotLights[ubo.spotLightCount].color          = glm::vec4(obj->color, obj->spotLight->intensity);
        ubo.spotLights[ubo.spotLightCount].outerCutoff    = glm::cos(glm::radians(obj->spotLight->outerCutoffAngle));
        ubo.spotLights[ubo.spotLightCount].constantAtten  = obj->spotLight->constantAttenuation;
        ubo.spotLights[ubo.spotLightCount].linearAtten    = obj->spotLight->linearAttenuation;
        ubo.spotLights[ubo.spotLightCount].quadraticAtten = obj->spotLight->quadraticAttenuation;
        ubo.spotLightCount++;
      }
    }
    else
    {
      // Legacy: iterate through all game objects (fallback)
      for (auto& [id, obj] : frameInfo.objectManager->getAllObjects())
      {
        // Point lights
        if (obj.pointLight)
        {
          assert(ubo.pointLightCount < maxLightCount && "Exceeded maximum point light count!");
          obj.transform.translation                     = glm::vec3(rotateLight * glm::vec4{obj.transform.translation, 1.f});
          ubo.pointLights[ubo.pointLightCount].position = glm::vec4(obj.transform.translation, 1.f);
          ubo.pointLights[ubo.pointLightCount].color    = glm::vec4(obj.color, obj.pointLight->intensity);
          ubo.pointLightCount++;
        }

        // Directional lights
        if (obj.directionalLight)
        {
          assert(ubo.directionalLightCount < maxLightCount && "Exceeded maximum directional light count!");

          // Update rotation to look at target if enabled
          if (obj.directionalLight->useTargetPoint)
          {
            obj.transform.lookAt(obj.directionalLight->targetPoint);
          }

          glm::vec3 direction                                        = obj.transform.getForwardDir();
          ubo.directionalLights[ubo.directionalLightCount].direction = glm::vec4(glm::normalize(direction), 0.f);
          ubo.directionalLights[ubo.directionalLightCount].color     = glm::vec4(obj.color, obj.directionalLight->intensity);
          ubo.directionalLightCount++;
        }

        // Spot lights
        if (obj.spotLight)
        {
          assert(ubo.spotLightCount < maxLightCount && "Exceeded maximum spot light count!");

          // Update rotation to look at target if enabled
          if (obj.spotLight->useTargetPoint)
          {
            obj.transform.lookAt(obj.spotLight->targetPoint);
          }

          glm::vec3 direction = obj.transform.getForwardDir();

          ubo.spotLights[ubo.spotLightCount].position       = glm::vec4(obj.transform.translation, 1.f);
          ubo.spotLights[ubo.spotLightCount].direction      = glm::vec4(glm::normalize(direction), glm::cos(glm::radians(obj.spotLight->innerCutoffAngle)));
          ubo.spotLights[ubo.spotLightCount].color          = glm::vec4(obj.color, obj.spotLight->intensity);
          ubo.spotLights[ubo.spotLightCount].outerCutoff    = glm::cos(glm::radians(obj.spotLight->outerCutoffAngle));
          ubo.spotLights[ubo.spotLightCount].constantAtten  = obj.spotLight->constantAttenuation;
          ubo.spotLights[ubo.spotLightCount].linearAtten    = obj.spotLight->linearAttenuation;
          ubo.spotLights[ubo.spotLightCount].quadraticAtten = obj.spotLight->quadraticAttenuation;
          ubo.spotLightCount++;
        }
      }
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
    pipelineConfig.renderPass                 = renderPass;
    pipelineConfig.pipelineLayout             = spotPipelineLayout;
    pipelineConfig.inputAssemblyInfo.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;

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
