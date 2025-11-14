#include "3dEngine/systems/PointLightSystem.hpp"

#include "3dEngine/Exceptions.hpp"

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

#include "3dEngine/systems/PointLightSystem.hpp"

namespace engine {
  struct PointLightPushConstants
  {
    /* data */
    glm::vec4 position{}; // w component unused
    glm::vec4 color{};    // w component is intensity
    float     radius{};
  };

  PointLightSystem::PointLightSystem(Device& device, VkRenderPass renderPass, VkDescriptorSetLayout globalSetLayout) : device(device)
  {
    createPipelineLayout(globalSetLayout);
    createPipeline(renderPass);
  }

  void PointLightSystem::createPipelineLayout(VkDescriptorSetLayout globalSetLayout)
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
  PointLightSystem::~PointLightSystem()
  {
    vkDestroyPipelineLayout(device.device(), pipelineLayout, nullptr);
  }

  void PointLightSystem::createPipeline(VkRenderPass renderPass)
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

  void PointLightSystem::render(FrameInfo& frameInfo)
  {
    pipeline->bind(frameInfo.commandBuffer);

    vkCmdBindDescriptorSets(frameInfo.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &frameInfo.globalDescriptorSet, 0, nullptr);

    // somewhat inefficient to loop over all game objects and check for point light components
    for (const auto& [id, obj] : frameInfo.gameObjects)
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
  }

  void PointLightSystem::update(FrameInfo& frameInfo, GlobalUbo& ubo) const
  {
    ubo.lightCount   = 0;
    auto rotateLight = glm::rotate(glm::mat4(1.f), frameInfo.frameTime, glm::vec3(0.f, -1.f, 0.f));
    for (auto& [id, obj] : frameInfo.gameObjects)
    {
      assert(ubo.lightCount < maxLightCount && "Exceeded maximum point light count in scene!");
      if (!obj.pointLight) continue;
      obj.transform.translation                = glm::vec3(rotateLight * glm::vec4{obj.transform.translation, 1.f});
      ubo.pointLights[ubo.lightCount].position = glm::vec4(obj.transform.translation, 1.f);
      ubo.pointLights[ubo.lightCount].color    = glm::vec4(obj.color, obj.pointLight->intensity);
      ubo.lightCount++;
    }
  }
} // namespace engine
